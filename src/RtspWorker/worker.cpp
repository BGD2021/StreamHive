#include "worker.hpp"
#include "config/config.hpp"
#include "stream/avPullStream.hpp"
#include "stream/avPushStream.hpp"
/*
worker工作流程：
1.启动拉流进程，拉取rtsp流并在on_track_frame_out回调函数中将frame放入待解码SafeQueue。
2.启动解码线程，从安全队列中取出frame进行解码，设置解码回调函数，在回调函数中将解码后的帧放入待推理SafeQueue。
3.启动推理线程池，从待推理SafeQueue中取出帧进行YOLO推理，处理完后将结果放入待编码SafeQueue。
4.启动编码线程，从待编码SafeQueue中取出帧进行编码，并推
送到指定的RTSP服务器。
5.在主线程中等待用户输入，按任意键退出后，清理所有线程和资源。
*/

RtspWorker::RtspWorker(std::string stream_name,std::string stream_url, int port, std::string push_path_first, std::string push_path_second, std::string model_root, int thread_num,msgServer *alarm_server)
    : stream_url(stream_url), port(port), push_path_first(push_path_first), push_path_second(push_path_second)
{
    ctx_ = new av_worker_context_t();
    const char *model_file = model_root.c_str();
    ctx_->pool = new framePool(model_file, thread_num);
    
    // 初始化Mat内存池
    ctx_->mat_pool = new MatPool(50, 10); // 最大50个，初始10个
    
    // 预分配常见分辨率的Mat对象
    // ctx_->mat_pool->preallocate(1920, 1080, 5);  // 1080p
    ctx_->mat_pool->preallocate(1280, 720, 5);   // 720p
    ctx_->mat_pool->preallocate(640, 480, 5);    // 480p
    
    // ctx_->frame_queue = new SafeQueue<std::shared_ptr<cv::Mat>>();
    this->stream_url = stream_url;
    ctx_->alarm_server = alarm_server; // 设置报警服务器
    ctx_->stream_name = stream_name; // 设置流名称
}

RtspWorker::~RtspWorker()
{
    stop();

    // 统一释放所有 ctx 内的资源
    if (ctx_)
    {
        if (ctx_->encoder)
        {
            delete ctx_->encoder;
            ctx_->encoder = nullptr;
        }
        if (ctx_->decoder)
        {
            delete ctx_->decoder;
            ctx_->decoder = nullptr;
        }
        if (ctx_->pool)
        {
            delete ctx_->pool;
            ctx_->pool = nullptr;
        }
        if (ctx_->mat_pool)
        {
            // 打印内存池统计信息
            ctx_->mat_pool->printStats();
            delete ctx_->mat_pool;
            ctx_->mat_pool = nullptr;
        }
        // if (ctx_->frame_queue)
        // {
        //     delete ctx_->frame_queue;
        //     ctx_->frame_queue = nullptr;
        // }
        if (ctx_->tracks)
        {
            mk_track_unref(ctx_->tracks);
            ctx_->tracks = nullptr;
        }
        delete ctx_;
        ctx_ = nullptr;
    }
}

void RtspWorker::start()
{
    printf("启动 RtspWorker...\n");
    pullStream_ = new AvPullStream(stream_url, ctx_);
    pull_thread_ = std::thread([this]()
                               { pullStream_->start(); });
    printf("拉流线程已启动\n");

    pushStream_ = new AvPushStream(ctx_, port, push_path_first, push_path_second);
    push_thread_ = std::thread([this]()
                               { pushStream_->pushSteamThread(); });
    printf("推流线程已创建\n");
    ctx_->running = true; // 设置运行标志为true
}

void RtspWorker::stop()
{
    printf("开始停止 RtspWorker...\n");

    // 设置停止标志
    ctx_->running = false;

    // 停止拉流
    if (pullStream_)
    {
        pullStream_->stop();
    }

    printf("等待线程结束...\n");

    // 等待线程结束
    if (pull_thread_.joinable())
    {
        pull_thread_.join();
        printf("拉流线程已结束\n");
    }
    if (push_thread_.joinable())
    {
        push_thread_.join();
        printf("推流线程已结束\n");
    }
    

    // 释放对象（但不释放 ctx 内部资源，留给析构函数处理）
    if (pullStream_)
    {
        delete pullStream_;
        pullStream_ = nullptr;
        printf("拉流对象已释放\n");
    }
    if (pushStream_)
    {
        delete pushStream_;
        pushStream_ = nullptr;
        printf("推流对象已释放\n");
    }

    printf("RtspWorker 停止完成\n");
}

bool RtspWorker::isRunning()
{
    return ctx_->running;
}
