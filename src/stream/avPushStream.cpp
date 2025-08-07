#include "avPushStream.hpp"

void API_CALL on_mk_media_source_regist_func(void *user_data, mk_media_source sender, int regist);

AvPushStream::AvPushStream(av_worker_context_t *ctx, int port, std::string push_path_first, std::string push_path_second)
    : ctx_(ctx), push_path_first(push_path_first), push_path_second(push_path_second)
{

    ctx->push_url = "rtsp://localhost/live/stream";
}

void release_pusher(mk_pusher *ptr)
{
    if (ptr && *ptr)
    {
        mk_pusher_release(*ptr);
        *ptr = nullptr;
    }
}
void release_media(mk_media *ptr)
{
    if (ptr && *ptr)
    {
        mk_media_release(*ptr);
        *ptr = nullptr;
    }
}
void release_track(mk_track *ptr)
{
    if (ptr && *ptr)
    {
        mk_track_unref(*ptr);
        *ptr = nullptr;
    }
}

void API_CALL on_mk_push_event_func(void *user_data, int err_code, const char *err_msg)
{
    av_worker_context_t *ctx = (av_worker_context_t *)user_data;
    if (err_code == 0)
    {
        // push success
        // log_info("push %s success!", ctx->push_url);
        // log_info("push success!");
        printf("push %s success!\n", ctx->push_url);
    }
    else
    {
        log_warn("push failed:%d %s", err_code, err_msg);
        printf("push failed:%d %s\n", err_code, err_msg);
        release_pusher(&(ctx->pusher));
    }
}

void API_CALL on_mk_media_source_regist_func(void *user_data, mk_media_source sender, int regist)
{
    av_worker_context_t *ctx = (av_worker_context_t *)user_data;
    const char *schema = mk_media_source_get_schema(sender);
    // 只处理 rtsp schema 的媒体源
    if (strcmp(schema, "rtsp") == 0)
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
    // 对于其他 schema（fmp4, rtmp, ts 等），直接忽略，不输出警告
}

void AvPushStream::pushSteamThread()
{
    void *mpp_frame = NULL;
    int mpp_frame_fd = 0;
    void *mpp_frame_addr = NULL;
    int enc_data_size;
    int frame_index = 0;

    // 等待编码器初始化完成,encoder已经在解码回调中初始化
    while (ctx_->encoder == NULL && ctx_->running && ctx_->tracks == nullptr)
    {
        printf("Waiting for encoder initialization...\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (ctx_->encoder == NULL)
    {
        printf("Encoder initialization failed!\n");
        return;
    }

    printf("Encoder initialized, starting encoding loop\n");

    printf("当前video width:%d\n", mk_track_video_width(ctx_->tracks));
    printf("当前push url：%s, push_path_first:%s, push_path_second:%s\n", ctx_->push_url, push_path_first.c_str(), push_path_second.c_str());
    media = mk_media_create("__defaultVhost__", push_path_first.c_str(), push_path_second.c_str(), 0, 0, 0);
    mk_media_init_track(media, ctx_->tracks);
    mk_media_init_complete(media);
    mk_media_set_on_regist(media, on_mk_media_source_regist_func, ctx_);
    while (ctx_->running)
    {
        int ret = 0;
        detection_t result = ctx_->pool->GetImageResultFromQueue();

        if (result.src == nullptr || result.src->data == nullptr) // 检查结果图像是否有效
        {
            // printf("result.src is nullptr or data is nullptr!\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        // printf("queue size: %d\n", ctx_->pool->GetResultQueueSize());
        // printf("Pool task size: %d\n", ctx_->pool->GetTasksSize());
        rga_buffer_t result_img = wrapbuffer_virtualaddr((void *)result.src->data, result.src->cols, result.src->rows, RK_FORMAT_RGB_888);
        if (result_img.vir_addr == nullptr) // 检查结果图像是否有效
        {
            // printf("result_img is nullptr!\n");
            continue;
        }
        if (result.objects->size() > 0 && result.objects->size() < 1000) // 检查检测结果是否有效
        {
            ctx_->alarm_server->sendAlarm(result.objects, ctx_->stream_name);
        }

        // printf("result_img vir_addr:%p\n", result_img.vir_addr);
        // 编码
        int enc_buf_size = ctx_->encoder->GetFrameSize();
        // 使用posix_memalign分配对齐内存，不要使用malloc
        char *enc_data = (char *)malloc(enc_buf_size);

        // 获取解码后的帧
        mpp_frame = ctx_->encoder->GetInputFrameBuffer();
        // 获取解码后的帧fd
        mpp_frame_fd = ctx_->encoder->GetInputFrameBufferFd(mpp_frame);
        // 获取解码后的帧地址
        mpp_frame_addr = ctx_->encoder->GetInputFrameBufferAddr(mpp_frame);
        // 这个是写入解码器的对象和颜色转换没有关系
        rga_buffer_t src = wrapbuffer_fd(mpp_frame_fd, ctx_->width, ctx_->height, RK_FORMAT_YCbCr_420_SP, ctx_->width_stride, ctx_->height_stride);
        frame_index++;
        // 结束计时
        auto now = std::chrono::steady_clock::now();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        imcopy(result_img, src);
        if (frame_index == 1)
        {
            enc_data_size = ctx_->encoder->GetHeader(enc_data, enc_buf_size);
        }
        enc_data_size = ctx_->encoder->Encode(mpp_frame, enc_data, enc_buf_size);
        // 推流
        //  printf("enc_data_size:%d\n", enc_data_size);
        if (enc_data_size <= 0)
        {
            printf("encode frame failed, enc_data_size:%d\n", enc_data_size);
            if (enc_data != nullptr)
            {
                free(enc_data);
                enc_data = nullptr;
            }
            continue;
        }
        ret = mk_media_input_h264(media, enc_data, enc_data_size, millis, millis);
        if (ret != 1)
        {
            printf("mk_media_input_frame failed\n");
        }
        // 确保内存被正确释放
        if (enc_data != nullptr)
        {
            free(enc_data);
            enc_data = nullptr;
        }
    }

    // 清理工作
    if (mpp_frame != NULL)
    {
        ctx_->encoder->ImportBuffer(0, 0, 0, 0);
        mpp_frame = NULL;
    }
    printf("推流线程已退出\n");
}

AvPushStream::~AvPushStream()
{
    if (ctx_->pusher)
    {
        mk_pusher_release(ctx_->pusher);
        ctx_->pusher = nullptr;
    }
    if (media)
    {
        mk_media_release(media);
        media = nullptr;
    }
}
