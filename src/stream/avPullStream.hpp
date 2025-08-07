#ifndef _AVPULLSTREAM_HPP_
#define _AVPULLSTREAM_HPP_

#include "mk_mediakit.h"
#include "threadPool/safeQueue.hpp"
#include "RtspWorker/worker.hpp"

/*功能：
拉流线程，通过zlmediaKit拉取RTSP流，获得视频编码类型和帧率等信息，并将拉取到的帧数据放入安全队列供后续处理。
*/

class AvPullStream {
public:
    AvPullStream(std::string url, av_worker_context_t* ctx);
    ~AvPullStream();
    bool start();
    void stop();
private:
    std::string url_;
    int video_type_;
    int video_fps_;
    mk_player player_;
    av_worker_context_t* ctx_; // 新增：指向外部ctx
};

#endif