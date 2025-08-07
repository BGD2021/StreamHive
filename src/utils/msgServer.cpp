#include "msgServer.hpp"
#include <iostream>

msgServer::msgServer(int rtspPort,int alarmPort,Config &config)
:rtspPort_(rtspPort), alarmPort_(alarmPort), config_(config)
{
    //创建内存
    rtsp_context = zmq_ctx_new();
    alarm_context = zmq_ctx_new();
    rtsp_socket = zmq_socket(rtsp_context, ZMQ_PUB);
    alarm_socket = zmq_socket(alarm_context, ZMQ_PUB);
    //设置套接字选项
    int rc = zmq_bind(rtsp_socket, ("tcp://*:" + std::to_string(rtspPort_)).c_str());
    if (rc != 0) {
       printf("Error binding RTSP socket: %s\n", zmq_strerror(zmq_errno()));
    } else {
       printf("RTSP server started on port %d\n", rtspPort_);    
    }

    rc = zmq_bind(alarm_socket, ("tcp://*:" + std::to_string(alarmPort_)).c_str());
    if (rc != 0) {
       printf("Error binding Alarm socket: %s\n", zmq_strerror(zmq_errno()));
    } else {
       printf("Alarm server started on port %d\n", alarmPort_);
    }
}

msgServer::~msgServer() {
    // 清理资源
    running = false; // 停止所有线程
    zmq_close(rtsp_socket);
    zmq_close(alarm_socket);
    zmq_ctx_destroy(rtsp_context);
    zmq_ctx_destroy(alarm_context);
    printf("Message server stopped\n");
}

void msgServer::sendMsg(void *socket,const std::string &msg) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    zmq_send(socket, msg.c_str(), msg.size(), 0);
}

void msgServer::setAlarmInterval(int interval_seconds) {
    alarm_interval_seconds_ = interval_seconds;
    printf("Alarm interval set to %d seconds\n", alarm_interval_seconds_);
}

void msgServer::sentRtspAddressThread(int interval_seconds) {
    while (running) {
        for (const auto &stream : config_.streams) {
            if (stream.enable) {
                // 构造包含流名字和RTSP地址的JSON格式消息
                std::string rtsp_url = "rtsp://192.168.10.107:" + std::to_string(config_.rtsp_server.port) + "/" + stream.output_app + "/" + stream.output_stream;
                
                // JSON格式：{"name":"流名字","url":"rtsp://...","id":"stream_id"}
                std::string json_msg = "{"
                    "\"name\":\"" + stream.name + "\","
                    "\"url\":\"" + rtsp_url + "\","
                    "\"id\":\"" + stream.id + "\""
                    "}";
                
                sendMsg(rtsp_socket,json_msg);
                printf("发送RTSP地址: %s\n", json_msg.c_str());
                std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
            }
        }
    }
}

void msgServer::sendAlarm(std::shared_ptr<std::vector<Detection>> detections, std::string stream_id) {
    //将报警信息放入队列
    AlarmMessage_t alarm_msg;
    alarm_msg.detections = detections;
    alarm_msg.stream_id = stream_id;
    alarm_queue_.push(alarm_msg);
}

void msgServer::sentAlarmThread() {
    while (running) {
        AlarmMessage_t alarm_msg;
        if (alarm_queue_.pop(alarm_msg)) {
            printf("接收到报警信息");
            // 构造报警消息
            std::string alarm_msg_str  = alarm_msg.stream_id + " 检测到 ";
            for (const auto &detection : *alarm_msg.detections) {
                alarm_msg_str += detection.className + " (" + std::to_string(detection.confidence) + "), ";
            }
            // 发送报警消息
            sendMsg(alarm_socket,alarm_msg_str);
            printf("发送报警信息: %s\n", alarm_msg_str.c_str());
            std::this_thread::sleep_for(std::chrono::seconds(alarm_interval_seconds_));
        }
    }
}

void msgServer::start() {
    // 启动RTSP地址发送线程
    rtsp_thread_ = std::thread(&msgServer::sentRtspAddressThread, this,1);
    // 启动报警信息发送线程
    alarm_thread_ = std::thread(&msgServer::sentAlarmThread, this);
    running = true;
    printf("Message server started\n");

}

void msgServer::stop() {
    running = false;
    // 等待线程结束
    if (rtsp_thread_.joinable()) {
        rtsp_thread_.join();
    }
    if (alarm_thread_.joinable()) {
        alarm_thread_.join();
    }
    printf("Message server stopped\n");
    zmq_close(rtsp_socket);
    zmq_close(alarm_socket);
    zmq_ctx_destroy(rtsp_context);
    zmq_ctx_destroy(alarm_context);

}


// msgServer::msgServer(int port) {
//     context = zmq_ctx_new();
//     socket = zmq_socket(context, ZMQ_PUB);
//     int rc = zmq_bind(socket, ("tcp://*:" + std::to_string(port)).c_str());
//     if (rc != 0) {
//         std::cerr << "Error starting message server: " << zmq_strerror(zmq_errno()) << std::endl;
//     } else {
//         std::cout << "Message server started on port 5555" << std::endl;
//     }
// }

// msgServer::~msgServer() {
//     zmq_close(socket);
//     zmq_ctx_destroy(context);
//     last_send_time_map_.clear(); // 清理时间记录
//     std::cout << "Message server stopped" << std::endl;
// }

// void msgServer::sendRtspAddress(const std::string &url) {
//     std::lock_guard<std::mutex> lock(send_mutex_);  // 保护ZMQ socket操作
//     int rc = zmq_send(socket, url.c_str(), url.size(), 0);
//     if (rc == -1) {
//         std::cerr << "Error sending RTSP address: " << zmq_strerror(zmq_errno()) << std::endl;
//     }
// }

// void msgServer::sendRtspAddressThread(Config &config,int interval_seconds) {
//     while (true) {
//         for (const auto &stream : config.streams) {
//             if (stream.enable) {
//                 // 构造包含流名字和RTSP地址的JSON格式消息
//                 std::string rtsp_url = "rtsp://192.168.10.107:" + std::to_string(config.rtsp_server.port) + "/" + stream.output_app + "/" + stream.output_stream;
                
//                 // JSON格式：{"name":"流名字","url":"rtsp://...","id":"stream_id"}
//                 std::string json_msg = "{"
//                     "\"name\":\"" + stream.name + "\","
//                     "\"url\":\"" + rtsp_url + "\","
//                     "\"id\":\"" + stream.id + "\""
//                     "}";
                
//                 sendRtspAddress(json_msg);
//                 std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
//             }
//         }
//     }
// }

// // 使用定时器或记录上次发送时间，避免阻塞推流线程
// void msgServer::sendAlarm(std::shared_ptr<std::vector<Detection>> detections, std::string stream_id, int interval_seconds) {
//     auto now = std::chrono::steady_clock::now();
    
//     // 保护报警时间记录的访问
//     {
//         std::lock_guard<std::mutex> lock(alarm_time_mutex_);
//         auto &last_send_time = last_send_time_map_[stream_id];
        
//         if (now - last_send_time < std::chrono::seconds(interval_seconds)) {
//             // printf("报警发送间隔未到，跳过发送: 流ID: %s\n", stream_id.c_str());
//             return; // 还在间隔期内，不发送报警
//         }
        
//         last_send_time = now; // 更新时间
//     }
//     // printf("正在发送报警信息: 流ID: %s\n", stream_id.c_str());
//     // 构造报警消息（在锁外进行，减少锁持有时间）
//     std::string alarm_msg = "流ID：" + stream_id + " 检测到目标";
//     for (const auto &detection : *detections) {
//         alarm_msg += " 类别：" + detection.className + " 置信度：" + std::to_string(detection.confidence) + ";";
//     }
    
//     sendRtspAddress(alarm_msg);
// }

