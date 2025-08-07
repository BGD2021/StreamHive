/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <opencv2/opencv.hpp>

#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"

#include "rkmedia/utils/mpp_decoder.h"
#include "rkmedia/utils/mpp_encoder.h"

#include "mk_mediakit.h"

#include "model/yolov5.h"
#include "utils/logging.h"
#include "draw/cv_draw.h"

#include "threadPool/framePool.hpp"
#include "threadPool/safeQueue.hpp"

#include "config/config.hpp"

#define THREADPOOL
typedef struct
{
    MppDecoder *decoder;
    MppEncoder *encoder;
    mk_media media;
    mk_pusher pusher;
    const char *push_url;

    int video_type=264;

    int push_rtsp_port;
    std::string push_path_first;
    std::string push_path_second;
    mk_player player;//拉流播放器对象
    framePool *pool; // 线程池对象

    /*生产者消费者模型*/
    SafeQueue<std::shared_ptr<cv::Mat>>* frame_queue; // 新增
    std::thread* consumer_thread; // 新增
    std::atomic<bool> running;    // 新增
    int width; // 视频宽度
    int height; // 视频高度
    int width_stride; // 视频宽度步长
    int height_stride; // 视频高度步长

} rknn_app_context_t;



void release_media(mk_media *ptr)
{
    if (ptr && *ptr)
    {
        mk_media_release(*ptr);
        *ptr = NULL;
    }
}

void release_pusher(mk_pusher *ptr)
{
    if (ptr && *ptr)
    {
        mk_pusher_release(*ptr);
        *ptr = NULL;
    }
}

void release_track(mk_track *ptr)
{
    if (ptr && *ptr)
    {
        mk_track_unref(*ptr);
        *ptr = NULL;
    }
}


void API_CALL on_mk_push_event_func(void *user_data, int err_code, const char *err_msg)
{
    rknn_app_context_t *ctx = (rknn_app_context_t *)user_data;
    if (err_code == 0)
    {
        // push success
        log_info("push %s success!", ctx->push_url);
        printf("push %s success!\n", ctx->push_url);
    }
    else
    {
        log_warn("push %s failed:%d %s", ctx->push_url, err_code, err_msg);
        printf("push %s failed:%d %s\n", ctx->push_url, err_code, err_msg);
        release_pusher(&(ctx->pusher));
    }
}

void API_CALL on_mk_media_source_regist_func(void *user_data, mk_media_source sender, int regist)
{
    rknn_app_context_t *ctx = (rknn_app_context_t *)user_data;
    const char *schema = mk_media_source_get_schema(sender);
    if (strncmp(schema, ctx->push_url, strlen(schema)) == 0)
    {
        release_pusher(&(ctx->pusher));
        if (regist)
        {
            ctx->pusher = mk_pusher_create_src(sender);
            mk_pusher_set_on_result(ctx->pusher, on_mk_push_event_func, ctx);
            mk_pusher_set_on_shutdown(ctx->pusher, on_mk_push_event_func, ctx);
            log_info("push started!");
            printf("push started!\n");
        }
        else
        {
            log_info("push stoped!");
            printf("push stoped!\n");
        }
        printf("push_url:%s\n", ctx->push_url);
    }
    else
    {
        printf("unknown schema:%s\n", schema);
    }
}

void API_CALL on_mk_shutdown_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[], int track_count)
{
    printf("play interrupted: %d %s", err_code, err_msg);
}

static std::deque<std::chrono::steady_clock::time_point> prod_fps_window;
static double prod_real_fps = 0;
// 解码后的数据回调函数
void mpp_decoder_frame_callback(void *userdata, int width_stride, int height_stride, int width, int height, int format, int fd, void *data)
{

    rknn_app_context_t *ctx = (rknn_app_context_t *)userdata;
    int ret = 0;
    // rga原始数据
    rga_buffer_t origin;
    // rga_buffer_t src;
    ctx->width = width;
    ctx->height = height;
    ctx->width_stride = width_stride;
    ctx->height_stride = height_stride;

    // 编码器准备
    if (ctx->encoder == NULL)
    {
        MppEncoder *mpp_encoder = new MppEncoder();
        MppEncoderParams enc_params;
        memset(&enc_params, 0, sizeof(MppEncoderParams));
        enc_params.width = width;
        enc_params.height = height;
        enc_params.hor_stride = width_stride;
        enc_params.ver_stride = height_stride;
        enc_params.fmt = MPP_FMT_YUV420SP;
        enc_params.type = MPP_VIDEO_CodingAVC;
        mpp_encoder->Init(enc_params, NULL);
        ctx->encoder = mpp_encoder;
    }


    // 复制到另一个缓冲区，避免修改mpp解码器缓冲区
    // 使用的是RK RGA的格式转换：YUV420SP -> RGB888
    origin = wrapbuffer_fd(fd, width, height, RK_FORMAT_YCbCr_420_SP, width_stride, height_stride);
    // 创建一块Mat格式的内存
    cv::Mat origin_mat = cv::Mat::zeros(height, width, CV_8UC3);
    //将这块内存包装成 RGA 目标图像
    rga_buffer_t rgb_img = wrapbuffer_virtualaddr((void *)origin_mat.data, width, height, RK_FORMAT_RGB_888);
    //填充 RGA 目标图像
    imcopy(origin, rgb_img);
    // 将Mat放入安全队列

    ctx->frame_queue->push(std::make_shared<cv::Mat>(origin_mat));
    // printf("[Producer] frame_queue size: %zu\n", ctx->frame_queue->size());
    // 在 mpp_decoder_frame_callback 里加
    
    auto now_tp = std::chrono::steady_clock::now();
    prod_fps_window.push_back(now_tp);
    const size_t FPS_WINDOW = 30;
    if (prod_fps_window.size() > FPS_WINDOW) {
        prod_fps_window.pop_front();
    }
    if (prod_fps_window.size() > 1) {
        double duration = std::chrono::duration_cast<std::chrono::milliseconds>(prod_fps_window.back() - prod_fps_window.front()).count() / 1000.0;
        if (duration > 0.0) {
            prod_real_fps = (prod_fps_window.size() - 1) / duration;
            printf("[Producer] FPS: %.2f\n", prod_real_fps);
        }
    }

//     // 将图像传入线程池进行处理
//     ctx->pool->inferenceThread(std::make_shared<cv::Mat>(origin_mat));
//     // 获取线程池处理后的结果
//     rga_buffer_t result_img = ctx->pool->GetImageResultFromQueue();
//     if (result_img.vir_addr == nullptr)  // 检查结果图像是否有效
//     {
//         printf("result_img is nullptr!\n");
//         return;
//     }
//     // rga_buffer_t processed_rgb = wrapbuffer_virtualaddr((void *)result_img->data, width, height, RK_FORMAT_RGB_888);
//     // 将当前时间点转换为毫秒级别的时间戳
//     auto millis = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
//     if (ret != 0)
//     {
//         printf("inference model fail\n");
//         goto RET;
//     }
//     imcopy(result_img, src);
//     memset(enc_data, 0, enc_buf_size);
//     enc_data_size = ctx->encoder->Encode(mpp_frame, enc_data, enc_buf_size);
//     ret = mk_media_input_h264(ctx->media, enc_data, enc_data_size, millis, millis);
//     if (ret != 1)
//     {
//         printf("mk_media_input_frame failed\n");
//     }

// RET: // tag
//     if (enc_data != nullptr)
//     {
//         free(enc_data);
//     }
}

void API_CALL on_track_frame_out(void *user_data, mk_frame frame)
{
    rknn_app_context_t *ctx = (rknn_app_context_t *)user_data;
    const char *data = mk_frame_get_data(frame);
    // ctx->dts = mk_frame_get_dts(frame);
    // ctx->pts = mk_frame_get_pts(frame);
    size_t size = mk_frame_get_data_size(frame);
    ctx->decoder->Decode((uint8_t *)data, size, 0);
    // mk_media_input_frame(ctx->media, frame);
}

void API_CALL on_mk_play_event_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[],
                                    int track_count)
{
    rknn_app_context_t *ctx = (rknn_app_context_t *)user_data;
    if (err_code == 0)
    {
        // success
        printf("play success!");
        int i;
        ctx->push_url = "rtmp://localhost/live/stream";
        ctx->media = mk_media_create("__defaultVhost__", ctx->push_path_first.c_str(), ctx->push_path_second.c_str(), 0, 0, 0);
        for (i = 0; i < track_count; ++i)
        {
            if (mk_track_is_video(tracks[i]))
            {
                log_info("got video track: %s", mk_track_codec_name(tracks[i]));
                // 监听track数据回调
                mk_media_init_track(ctx->media, tracks[i]);
                mk_track_add_delegate(tracks[i], on_track_frame_out, user_data);
            }
        }
        mk_media_init_complete(ctx->media);
        mk_media_set_on_regist(ctx->media, on_mk_media_source_regist_func, ctx);
    }
    else
    {
        printf("play failed: %d %s", err_code, err_msg);
    }
}


int rtsp2rtsp(rknn_app_context_t *ctx, const char *url){
     // MPP 解码器
    if (ctx->decoder == NULL)
    {
        MppDecoder *decoder = new MppDecoder();           // 创建解码器
        /*!目前写死30帧，后续应该从on_mk_play_event_func中通过mk_track_get_fps获取fps传入*/
        decoder->Init(ctx->video_type, 30, ctx);          // 初始化解码器
        decoder->SetCallback(mpp_decoder_frame_callback); // 设置回调函数，用来处理解码后的数据
        ctx->decoder = decoder;                        // 将解码器赋值给上下文
    }

    mk_player player = mk_player_create();
    ctx->player = player;
    mk_player_set_on_result(player, on_mk_play_event_func, ctx);
    mk_player_set_on_shutdown(player, on_mk_shutdown_func, ctx);
    mk_player_play(player, url);

    printf("enter any key to exit\n");
    getchar();

    if (player)
    {
        mk_player_release(player);
    }
    // 释放解码器
    if (ctx->decoder)
    {
        delete ctx->decoder;
        ctx->decoder = nullptr;
    }
    // 释放媒体对象
    if (ctx->media)
    {
        release_media(&ctx->media);
    }
    // 释放推流对象
    if (ctx->pusher)
    {
        release_pusher(&ctx->pusher);
    }
    // 释放线程池
    if (ctx->pool)
    {
        delete ctx->pool;
        ctx->pool = nullptr;
    }
    // 释放安全队列
    if (ctx->frame_queue)
    {
        delete ctx->frame_queue;
        ctx->frame_queue = nullptr;
    }
    // 释放消费者线程
    if (ctx->consumer_thread)
    {
        if (ctx->consumer_thread->joinable())
        {
            ctx->consumer_thread->join();
        }
        delete ctx->consumer_thread;
        ctx->consumer_thread = nullptr;
    }
    // 清理上下文
    memset(ctx, 0, sizeof(rknn_app_context_t));
    printf("exit successfully\n");
    return 0;
}


void consumer_func(rknn_app_context_t* ctx) {
    void *mpp_frame = NULL;
    int mpp_frame_fd = 0;
    void *mpp_frame_addr = NULL;
    int enc_data_size;
    const size_t FPS_WINDOW = 30; // 可调整窗口大小
    std::deque<std::chrono::steady_clock::time_point> fps_window;
    double real_fps = 0;
    double max_fps = 0,min_fps = 1000,avg_fps = 0;
    while (ctx->running) {
        int ret = 0;
        rga_buffer_t src;
        std::shared_ptr<cv::Mat> mat;
        //打印frame_queue的大小
        // printf("[Consumer] frame_queue size: %zu\n", ctx->frame_queue->size());

        if (ctx->frame_queue->pop(mat)) {
            // 推理
            ctx->pool->inferenceThread(mat);
            rga_buffer_t result_img = ctx->pool->GetImageResultFromQueue();
            if(result_img.vir_addr == nullptr)  // 检查结果图像是否有效
            {
                printf("result_img is nullptr!\n");
                continue;
            }
            // --- FPS滑动窗口统计 ---
            auto now_tp = std::chrono::steady_clock::now();
            fps_window.push_back(now_tp);
            if (fps_window.size() > FPS_WINDOW) {
                fps_window.pop_front();
            }
            if (fps_window.size() > 1) {
                double duration = std::chrono::duration_cast<std::chrono::milliseconds>(fps_window.back() - fps_window.front()).count() / 1000.0;
                if (duration > 0.0) {
                    real_fps = (fps_window.size() - 1) / duration;
                    //记录最高帧 最低帧 和平均帧数
                    max_fps = std::max(max_fps, real_fps);
                    min_fps = std::min(min_fps, real_fps);
                    avg_fps = (avg_fps * (fps_window.size() - 1) + real_fps) / fps_window.size();
                    printf("FPS: %.2f,Max FPS: %.2f, Min FPS: %.2f, Avg FPS: %.2f\n",real_fps, max_fps, min_fps, avg_fps);

                    // printf("FPS: %.2f\n", real_fps);
                    
                }
            }
            // 编码
            int enc_buf_size = ctx->encoder->GetFrameSize();
            char *enc_data = (char *)malloc(enc_buf_size);

            // 获取解码后的帧
            mpp_frame = ctx->encoder->GetInputFrameBuffer();
            // 获取解码后的帧fd
            mpp_frame_fd = ctx->encoder->GetInputFrameBufferFd(mpp_frame);
            // 获取解码后的帧地址
            mpp_frame_addr = ctx->encoder->GetInputFrameBufferAddr(mpp_frame);
            // 这个是写入解码器的对象和颜色转换没有关系
            src = wrapbuffer_fd(mpp_frame_fd, ctx->width, ctx->height, RK_FORMAT_YCbCr_420_SP, ctx->width_stride, ctx->height_stride);
            //     // 将当前时间点转换为毫秒级别的时间戳
            auto millis = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
            if (ret != 0)
            {
                printf("inference model fail\n");
                continue;
            }
            imcopy(result_img, src);
            memset(enc_data, 0, enc_buf_size);
            enc_data_size = ctx->encoder->Encode(mpp_frame, enc_data, enc_buf_size);
            //推流
            ret = mk_media_input_h264(ctx->media, enc_data, enc_data_size, millis, millis);
            if (ret != 1)
            {
                printf("mk_media_input_frame failed\n");
            }
            free(enc_data);
        }
    }
    // 清理工作
    if (mpp_frame != NULL) {
        ctx->encoder->ImportBuffer(0, 0, 0, 0);
        mpp_frame = NULL;   
    }
}

int main(int argc, char **argv)
{
    
	const char* file = NULL;

	for (int i = 1; i < argc; i += 2)
	{
		if (argv[i][0] != '-')
		{
			printf("parameter error:%s\n", argv[i]);
			return -1;
		}
		switch (argv[i][1])
		{
			case 'h': {
				//打印help信息
				printf("-h 打印参数配置信息并退出\n");
				printf("-f 配置文件    如：-f config.json \n");
				system("pause\n"); 
				exit(0); 
				return -1;
			}
			case 'f': {
				file = argv[i + 1];
				break;
			}
			default: {
				printf("set parameter error:%s\n", argv[i]);
				return -1;

			}
		}
	}
	
	if (file == NULL) {
		printf("failed to read config file\n");
		return -1;
	}

	Config file_config(file);
    if (!file_config.mState) {
		printf("failed to read config file: %s\n", file);
		return -1;
	}
    // 打印配置文件内容
    file_config.show();
    int status = 0;
    int ret;
    char *stream_url = argv[1];               // 视频流地址
    // stream_url = "rtsp://admin:qweasdzxc12@192.168.10.105/h264/ch1/main/av_stream";
    stream_url = const_cast<char*>(file_config.camera_rtsp.c_str());
    // stream_url = "rtsp://admin:qweasdzxc12@192.168.10.105:554/Streaming/Channels/101";
    // const char *model_file = argv[2];        //模型地址
    const char *model_file = file_config.model_root.c_str();

    //转为int
    // int thread_num_ = atoi(thread_num);
    int thread_num_ = file_config.thread_num;
    if(thread_num_ <= 0)
    {
        thread_num_ = 5; // 默认线程数
    }
    printf("thread_num: %d\n", thread_num_);
    int video_type = 264;

    // 初始化流媒体
    mk_config config;
    memset(&config, 0, sizeof(mk_config));
    config.log_mask = LOG_CONSOLE;
    config.thread_num = 4;
    mk_env_init(&config);
    mk_rtsp_server_start(3554, 0);

    rknn_app_context_t app_ctx;                      // 创建上下文
    memset(&app_ctx, 0, sizeof(rknn_app_context_t)); // 初始化上下文
    app_ctx.video_type = video_type;
    app_ctx.push_path_first = "live";
    app_ctx.push_path_second = "test";
    app_ctx.pool = new framePool(model_file, thread_num_); // 初始化线程池
    app_ctx.frame_queue = new SafeQueue<std::shared_ptr<cv::Mat>>();
    app_ctx.running = true;
    app_ctx.consumer_thread = new std::thread(consumer_func, &app_ctx);
    printf("线程池模式\n");
    rtsp2rtsp(&app_ctx, stream_url);
    
    printf("waiting finish\n");
    usleep(3 * 1000 * 1000);

    if (app_ctx.encoder != nullptr)
    {
        delete (app_ctx.encoder);
        app_ctx.encoder = nullptr;
    }
    if (app_ctx.pool != nullptr)
    {
        delete (app_ctx.pool);
        app_ctx.pool = nullptr;
    }
    app_ctx.running = false;
    if (app_ctx.consumer_thread->joinable()) app_ctx.consumer_thread->join();
    delete app_ctx.consumer_thread;
    delete app_ctx.frame_queue;

    return 0;
}