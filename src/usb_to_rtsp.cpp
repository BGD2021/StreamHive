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

typedef struct
{
    MppEncoder *encoder;
    mk_media media;
    mk_pusher pusher;
    const char *push_url;

    int video_type=264;

    int push_rtsp_port;
    std::string push_path_first;
    std::string push_path_second;


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



// 函数定义  
int padToMultipleOf16(int number) {  
    // 如果number已经是16的倍数，则直接返回  
    if (number % 16 == 0) {  
        return number;  
    }  
    // 否则，计算需要添加的额外量（即16 - (number % 16)）  
    // 这等价于找到比number大的最小16的倍数，并减去number  
    int extra = 16 - (number % 16);  
    // 返回扩充后的数  
    return number + extra;  
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

int process_video_file(rknn_app_context_t *ctx, const char *video_path)
{
    // 读取视频
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened())
    {
        printf("Failed to open video file: %s", video_path);
    }
    // 使用前需要使用v4l2-ctl --device=/dev/video0 --list-formats-ext检查一下设备支持范围
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    // set width
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
    // set height
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
    // set fps
    cap.set(cv::CAP_PROP_FPS,30);
    // 获取视频尺寸、帧率
    int cap_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int cap_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    int fps = cap.get(cv::CAP_PROP_FPS);

    ctx->push_url = "rtsp://localhost/live/stream";

    ctx->media = mk_media_create("__defaultVhost__", ctx->push_path_first.c_str(), ctx->push_path_second.c_str(), 0, 0, 0);

    codec_args v_args = {0};
    mk_track v_track = mk_track_create(MKCodecH264, &v_args);
    mk_media_init_track(ctx->media, v_track);

    mk_media_init_complete(ctx->media);
    mk_media_set_on_regist(ctx->media, on_mk_media_source_regist_func, ctx);

    // 初始化编码器
    MppEncoder *mpp_encoder = new MppEncoder();
    MppEncoderParams enc_params;
    memset(&enc_params, 0, sizeof(MppEncoderParams));
    enc_params.width = cap_width;
    enc_params.height = cap_height;
    enc_params.fmt = MPP_FMT_YUV420SP;
    enc_params.type = MPP_VIDEO_CodingAVC;
    mpp_encoder->Init(enc_params, ctx);
    ctx->encoder = mpp_encoder;

    // mpp编码配置
    void *mpp_frame = NULL;
    int mpp_frame_fd = 0;
    void *mpp_frame_addr = NULL;
    int enc_data_size;
    
    int frame_index = 0;
    int ret = 0;

    // 画面
    cv::Mat img;

    while (true)
    {
        // 读取视频帧
        cap >> img;
        if (img.empty())
        {
            printf("Video end.");
            break;
        }

        frame_index++;
        // 结束计时
        auto end_time = std::chrono::high_resolution_clock::now();
        // 将当前时间点转换为毫秒级别的时间戳
        auto millis = std::chrono::time_point_cast<std::chrono::milliseconds>(end_time).time_since_epoch().count();

        // 获取解码后的帧
        mpp_frame = ctx->encoder->GetInputFrameBuffer();
        // 获取解码后的帧fd
        mpp_frame_fd = ctx->encoder->GetInputFrameBufferFd(mpp_frame);
        // 获取解码后的帧地址
        mpp_frame_addr = ctx->encoder->GetInputFrameBufferAddr(mpp_frame);

        rga_buffer_t src = wrapbuffer_fd(mpp_frame_fd, cap_width, cap_height, RK_FORMAT_YCbCr_420_SP,padToMultipleOf16(cap_width),padToMultipleOf16(cap_height));

        int enc_buf_size = ctx->encoder->GetFrameSize();

        char *enc_data = (char *)malloc(enc_buf_size);

        rga_buffer_t rgb_img = wrapbuffer_virtualaddr((void *)img.data, cap_width, cap_height, RK_FORMAT_BGR_888);
        // 将RGB图像复制到src中
        imcopy(rgb_img, src);

        if (frame_index == 1)
        {
            enc_data_size = ctx->encoder->GetHeader(enc_data, enc_buf_size);
        }
        // 内存初始化
        memset(enc_data, 0, enc_buf_size);
        
        enc_data_size = ctx->encoder->Encode(mpp_frame, enc_data, enc_buf_size);

        ret = mk_media_input_h264(ctx->media, enc_data, enc_data_size, millis, millis);
        if (ret != 1)
        {
            printf("mk_media_input_frame failed\n");
        }
        if (enc_data != nullptr)
        {
            free(enc_data);
        }
    }
    // 释放资源
    cap.release();
    release_track(&v_track);
    release_media(&ctx->media);

}

int main(int argc, char **argv)
{
    int status = 0;
    int ret;

    if (argc != 2)
    {
        printf("Usage: %s<video_path>\n", argv[0]);
        return -1;
    }
    char *stream_url = argv[1];               // 视频流地址
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
    app_ctx.push_path_first = "yunyan-live";
    app_ctx.push_path_second = "test";

    process_video_file(&app_ctx, stream_url);

    printf("waiting finish\n");
    usleep(3 * 1000 * 1000);

    if (app_ctx.encoder != nullptr)
    {
        delete (app_ctx.encoder);
        app_ctx.encoder = nullptr;
    }

    return 0;
}