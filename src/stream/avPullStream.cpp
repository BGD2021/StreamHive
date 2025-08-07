#include "avPullStream.hpp"
#include "matPool.hpp"

void API_CALL on_mk_play_event_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[], int track_count);
void API_CALL on_mk_shutdown_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[], int track_count);
void API_CALL on_track_frame_out(void *user_data, mk_frame frame);
void mpp_decoder_frame_callback(void *userdata, int width_stride, int height_stride, int width, int height, int format, int fd, void *data);

AvPullStream::AvPullStream(std::string url, av_worker_context_t *ctx)
    : url_(url), ctx_(ctx), video_type_(264), video_fps_(30), player_(nullptr)
{
}
// 拉流回调
void API_CALL on_mk_play_event_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[],
                                    int track_count)
{
    av_worker_context_t *ctx = (av_worker_context_t *)user_data;
    if (err_code == 0)
    {
        // success
        printf("play success!\n");
        int i;
        for (i = 0; i < track_count; ++i)
        {
            if (mk_track_is_video(tracks[i]))
            {
                printf("got video track: %s\n", mk_track_codec_name(tracks[i]));
                // 将track信息保存到ctx，供推流使用
                ctx->tracks = mk_track_ref(tracks[i]);
                printf("get video track\n");
                ctx->video_type = 264;
                ctx->video_fps = mk_track_video_fps(tracks[i]);
                if (ctx->video_fps == 0)
                {
                    ctx->video_fps = 30; // 默认帧率
                }
                ctx->width = mk_track_video_width(tracks[i]);
                ctx->height = mk_track_video_height(tracks[i]);
                printf("video type: %d, fps: %d, width: %d, height: %d\n",
                       ctx->video_type, ctx->video_fps, ctx->width, ctx->height);
                if (ctx->decoder == NULL)
                {
                    MppDecoder *decoder = new MppDecoder(); // 创建解码器
                    /*!目前写死30帧，后续应该从on_mk_play_event_func中通过mk_track_get_fps获取fps传入*/
                    printf("当前type：%d，fps：%d\n", ctx->video_type, ctx->video_fps);
                    // decoder->Init(ctx->video_type, ctx->video_fps, ctx); // 初始化解码器
                    decoder->Init(ctx->video_type, 30, ctx); // 初始化解码器
                    decoder->SetCallback(mpp_decoder_frame_callback);    // 设置回调函数，用来处理解码后的数据
                    ctx->decoder = decoder;                              // 将解码器赋值给上下文
                }
                // 监听track数据回调
                mk_track_add_delegate(tracks[i], on_track_frame_out, user_data);
            }
        }
    }
    else
    {
        printf("play failed: %d %s", err_code, err_msg);
    }
}
// 播放器中断回调
void API_CALL on_mk_shutdown_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[], int track_count)
{
    printf("play interrupted: %d %s", err_code, err_msg);
}
// track中的帧数据回调
void API_CALL on_track_frame_out(void *user_data, mk_frame frame)
{
    av_worker_context_t *ctx = (av_worker_context_t *)user_data;
    const char *data = mk_frame_get_data(frame);
    size_t size = mk_frame_get_data_size(frame);
    ctx->decoder->Decode((uint8_t *)data, size, 0);
}

void mpp_decoder_frame_callback(void *userdata, int width_stride, int height_stride, int width, int height, int format, int fd, void *data)
{

    av_worker_context_t *ctx = (av_worker_context_t *)userdata;
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
    
    // 从内存池获取Mat对象，而不是每次都创建新的
    std::shared_ptr<cv::Mat> origin_mat;
    if (ctx->mat_pool != nullptr) {
        try {
            origin_mat = ctx->mat_pool->getMat(width, height, CV_8UC3);
        } catch (const std::exception& e) {
            printf("Failed to get Mat from pool: %s\n", e.what());
            // 降级到直接创建Mat
            origin_mat = std::make_shared<cv::Mat>(height, width, CV_8UC3);
        }
    } else {
        // 如果没有内存池，直接创建（保持向后兼容）
        origin_mat = std::make_shared<cv::Mat>(height, width, CV_8UC3);
    }
    
    // 将这块内存包装成 RGA 目标图像
    rga_buffer_t rgb_img = wrapbuffer_virtualaddr((void *)origin_mat->data, width, height, RK_FORMAT_RGB_888);
    // 填充 RGA 目标图像
    ret = imcopy(origin, rgb_img);
    if (ret != IM_STATUS_SUCCESS)
    {
        printf("imcopy failed! ret: %d, error: %s\n", ret, imStrError((IM_STATUS)ret));
        return;
    }
    /*直接将图片推入推理线程池，避免额外的队列和线程管理*/
    if (ctx->pool == nullptr)
    {
        printf("ctx->pool is nullptr!\n");
        return;
    }
    // printf("Pushing image to inference thread pool...\n");
    try
    {
        ctx->pool->inferenceThread(origin_mat);
    }
    catch (const std::bad_alloc &e)
    {
        printf("Memory allocation failed: %s\n", e.what());
        return;
    }
    // 直接在解码回调中提交推理任务，避免生命周期问题
    // ctx->pool->inferenceThread(std::make_shared<cv::Mat>(origin_mat.clone()));
    // ctx->pool->inferenceThread(std::make_shared<cv::Mat>(origin_mat));
}

AvPullStream::~AvPullStream()
{
    stop();
    if (player_)
    {
        mk_player_release(player_);
        player_ = nullptr;
    }
}

bool AvPullStream::start()
{
    // 初始化播放器
    player_ = mk_player_create();
    if (!player_)
    {
        log_error("Failed to create mk_player");
        return false;
    }
    // 设置回调函数
    mk_player_set_on_result(player_, on_mk_play_event_func, ctx_);
    mk_player_set_on_shutdown(player_, on_mk_shutdown_func, ctx_);
    // 初始化解码器

    if (!player_)
    {
        log_error("Player not initialized");
        return false;
    }
    // 启动播放器
    char *stream_url = const_cast<char *>(url_.c_str());
    mk_player_play(player_, stream_url);
    printf("Player started with URL: %s\n", stream_url);

    while (ctx_->running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}
void AvPullStream::stop()
{

    if (ctx_)
    {
        ctx_->running = false;
    }

    if (player_)
    {
        log_info("Stopping player...");
        mk_player_release(player_);
        player_ = nullptr;
    }
    printf("拉流已停止\n");
}