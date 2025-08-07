#ifndef MSGSERVER_HPP
#define MSGSERVER_HPP
#include <zmq.h>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "config/config.hpp"
#include "model/yolov5.h"
#include "threadPool/safeQueue.hpp"

// class msgServer
// {
// private:
//     void *context;
//     void *socket;
//     // Config config_; // 移除这个成员变量，避免生命周期问题
    
//     // 线程安全相关
//     mutable std::mutex send_mutex_;           // 保护 ZMQ socket 发送操作
//     mutable std::mutex alarm_time_mutex_;     // 保护报警时间记录
//     std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_send_time_map_;
    
// public:
//     msgServer(int port);
//     ~msgServer();
//     void sendRtspAddress(const std::string &url);
//     void sendRtspAddressThread(Config &config,int interval_seconds = 5);
//     void sendAlarm(std::shared_ptr<std::vector<Detection>>,std::string stream_id, int interval_seconds = 5);
// };

typedef struct {
    std::shared_ptr<std::vector<Detection>> detections; // 检测结果
    std::string stream_id; // 流ID
} AlarmMessage_t;

class msgServer {
public:
    msgServer(int rtspPort,int alarmPort,Config &config);
    ~msgServer();
    void sendMsg(void *socket,const std::string &msg);
    void sendAlarm(std::shared_ptr<std::vector<Detection>> detections, std::string stream_id);
    void setAlarmInterval(int interval_seconds);
    // void setAlarmClass();
    void sentRtspAddressThread(int interval_seconds=1);
    void sentAlarmThread();
    void start();
    void stop();
private:
    void *rtsp_context;
    void *alarm_context;
    void *rtsp_socket;    
    void *alarm_socket; 
    std::atomic<bool> running{false};
    int rtspPort_;
    int alarmPort_;
    int alarm_interval_seconds_{5}; // 报警间隔时间，默认5秒
    SafeQueue<AlarmMessage_t> alarm_queue_{1};
    Config config_; // 配置对象，用于获取流信息
    std::thread rtsp_thread_; // RTSP地址发送线程
    std::thread alarm_thread_; // 报警信息发送线程
    std::mutex send_mutex_;  // 保护ZMQ socket操作
};
#endif