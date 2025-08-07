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
    MppDecoder *decoder;
    MppEncoder *encoder;
    mk_media media;
    mk_pusher pusher;
    const char *push_url;

    mk_player player;
    // Yolov8Custom yolo;


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

// 解码后的数据回调函数
void mpp_decoder_frame_callback(void *userdata, int width_stride, int height_stride, int width, int height, int format, int fd, void *data)
{

    rknn_app_context_t *ctx = (rknn_app_context_t *)userdata;
    int ret = 0;
    // 帧画面计数
    static int frame_index = 0;
    frame_index++;

    void *mpp_frame = NULL;
    int mpp_frame_fd = 0;
    void *mpp_frame_addr = NULL;
    int enc_data_size;

    // rga原始数据aiqiyi
    rga_buffer_t origin;
    rga_buffer_t src;

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

    // 编码
    int enc_buf_size = ctx->encoder->GetFrameSize();
    char *enc_data = (char *)malloc(enc_buf_size);

    // 获取解码后的帧
    mpp_frame = ctx->encoder->GetInputFrameBuffer();
    // 获取解码后的帧fd
    mpp_frame_fd = ctx->encoder->GetInputFrameBufferFd(mpp_frame);
    // 获取解码后的帧地址
    mpp_frame_addr = ctx->encoder->GetInputFrameBufferAddr(mpp_frame);

    // 复制到另一个缓冲区，避免修改mpp解码器缓冲区
    // 使用的是RK RGA的格式转换：YUV420SP -> RGB888
    origin = wrapbuffer_fd(fd, width, height, RK_FORMAT_YCbCr_420_SP, width_stride, height_stride);
    // 这个是写入解码器的对象和颜色转换没有关系
    src = wrapbuffer_fd(mpp_frame_fd, width, height, RK_FORMAT_YCbCr_420_SP, width_stride, height_stride);
    // 创建一个等宽高的空对象
    cv::Mat origin_mat = cv::Mat::zeros(height, width, CV_8UC3);
    rga_buffer_t rgb_img = wrapbuffer_virtualaddr((void *)origin_mat.data, width, height, RK_FORMAT_RGB_888);
    imcopy(origin, rgb_img);

    // 将当前时间点转换为毫秒级别的时间戳
    auto millis = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
    if (ret != 0)
    {
        printf("inference model fail\n");
        goto RET;
    }
    imcopy(rgb_img, src);
    memset(enc_data, 0, enc_buf_size);
    enc_data_size = ctx->encoder->Encode(mpp_frame, enc_data, enc_buf_size);
    ret = mk_media_input_h264(ctx->media, enc_data, enc_data_size, millis, millis);
    if (ret != 1)
    {
        printf("mk_media_input_frame failed\n");
    }

RET: // tag
    if (enc_data != nullptr)
    {
        free(enc_data);
    }
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
    printf("mk_media_source:%x\n", sender);
    rknn_app_context_t *ctx = (rknn_app_context_t *)user_data;
    const char *schema = mk_media_source_get_schema(sender);
    if (strncmp(schema, ctx->push_url, strlen(schema)) == 0)
    {
        // 判断是否为推流协议相关的流注册或注销事件
        printf("schema: %s\n", schema);
        release_pusher(&(ctx->pusher));
        if (regist)
        {
            ctx->pusher = mk_pusher_create_src(sender);
            mk_pusher_set_on_result(ctx->pusher, on_mk_push_event_func, ctx);
            mk_pusher_set_on_shutdown(ctx->pusher, on_mk_push_event_func, ctx);
            //            mk_pusher_publish(ctx->pusher, ctx->push_url);
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

void API_CALL on_mk_shutdown_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[], int track_count)
{
    printf("play interrupted: %d %s", err_code, err_msg);
}

int process_video_rtsp(rknn_app_context_t *ctx, const char *url)
{

    // MPP 解码器
    if (ctx->decoder == NULL)
    {
        MppDecoder *decoder = new MppDecoder();           // 创建解码器
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
    return 0;
}

int main(int argc, char **argv)
{
    int status = 0;
    int ret;

    // if (argc != 2)
    // {
    //     printf("Usage: %s<video_path>\n", argv[0]);
    //     return -1;
    // }
    // char *stream_url = argv[1];               // 视频流地址
    char *stream_url = "rtsp://admin:qweasdzxc12@192.168.10.104/h264/ch1/main/av_stream";
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

    process_video_rtsp(&app_ctx, stream_url);

    printf("waiting finish\n");
    usleep(3 * 1000 * 1000);

    if (app_ctx.encoder != nullptr)
    {
        delete (app_ctx.encoder);
        app_ctx.encoder = nullptr;
    }

    return 0;
}