#ifndef _AVPUSHSTREAM_HPP_
#define _AVPUSHSTREAM_HPP_

#include "mk_mediakit.h"
#include "threadPool/safeQueue.hpp"
#include "RtspWorker/worker.hpp"
#include <opencv2/opencv.hpp>


class AvPushStream
{
public:
    AvPushStream(av_worker_context_t *ctx,int port,std::string push_path_first,std::string push_path_second);
    ~AvPushStream();
    void pushSteamThread();
    // void inferenceThread();

private:
    av_worker_context_t *ctx_; // 指向外部ctx
    std::string push_path_first;
    std::string push_path_second;
    mk_media media;
};

#endif