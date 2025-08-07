#ifndef RTSP_WORKER_HPP
#define RTSP_WORKER_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <opencv2/opencv.hpp>
#include "threadPool/safeQueue.hpp"
#include "threadPool/framePool.hpp"
#include "mk_mediakit.h"
#include "rkmedia/utils/mpp_decoder.h"
#include "rkmedia/utils/mpp_encoder.h"
#include "stream/matPool.hpp"
#include "utils/msgServer.hpp"

// #include "stream/avPullStream.hpp"
// #include "stream/avDecoder.hpp"
// #include "stream/avEncoder.hpp"
// #include "stream/avPushStream.hpp"

// 前置声明，避免循环依赖
class AvPullStream;
class AvPushStream;

/*
这个结构体用于保存视频流处理的上下文信息，有些信息需要从拉流类传递到推流类
*/


typedef struct {
    std::string stream_name; // 流名称
    int video_type;      // 编码类型
    int video_fps;       // 帧率
    int width;           // 视频宽度
    int height;          // 视频高度
    int width_stride;   // 视频宽度步长
    int height_stride;  // 视频高度步长
    const char *push_url;
    std::atomic<bool> running;
    
    MppDecoder *decoder;// 解码器对象
    MppEncoder *encoder;// 编码器对象
    mk_pusher pusher;
    mk_track tracks; // 视频轨道

    // SafeQueue<std::shared_ptr<cv::Mat>> *frame_queue; // 帧队列，用于存储待处理的帧

    framePool *pool; // 线程池对象
    MatPool *mat_pool; // Mat内存池对象
    msgServer *alarm_server; // 消息服务器，用于发送RTSP地址和报警信息
} av_worker_context_t;



class RtspWorker {
public:
    RtspWorker(std::string stream_name,std::string stream_url,int port, std::string push_path_first, std::string push_path_second,std::string model_root,int thread_num,msgServer *alarm_server = nullptr);
    ~RtspWorker();

    void start();    // 启动所有线程
    void stop();     // 停止线程并释放资源
    bool isRunning();
    std::atomic<bool> running_;

private:
    
    std::string stream_url; // RTSP流地址
    int port;
    std::string push_path_first;
    std::string push_path_second;

    AvPullStream *pullStream_; // 拉流对象
    AvPushStream *pushStream_; // 推流对象

    std::thread pull_thread_;
    std::thread push_thread_;
    

    av_worker_context_t *ctx_; // 新增：保存视频流相关信息
};

#endif // RTSP_WORKER_HPP