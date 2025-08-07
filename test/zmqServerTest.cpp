#include <zmq.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>

class ZmqTestServer {
private:
    void* context;
    void* rtsp_socket;    // 5555端口 - RTSP地址
    void* alarm_socket;   // 5556端口 - 报警信息
    std::atomic<bool> running{false};
    
    // 测试数据
    std::vector<std::string> rtsp_urls = {
        "rtsp://192.168.10.107:3554/live/camera01",
        "rtsp://192.168.10.107:3554/live/camera02",
        "rtsp://192.168.10.107:3554/live/camera03",
        "rtsp://192.168.10.107:3554/live/camera04"
    };
    
    std::vector<std::string> stream_ids = {
        "后门摄像头",
        "前门摄像头",
        "侧门摄像头",
        "大厅摄像头"
    };
    
    std::vector<std::string> class_names = {
        "person",
        "car",
        "tie",
        "stop sign",
        "bicycle",
        "motorcycle",
        "bus",
        "truck"
    };
    
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> confidence_dist;
    std::uniform_int_distribution<int> stream_dist;
    std::uniform_int_distribution<int> class_dist;

public:
    ZmqTestServer() : gen(rd()), confidence_dist(0.2f, 0.95f), 
                      stream_dist(0, stream_ids.size()-1), 
                      class_dist(0, class_names.size()-1) {
        
        // 初始化ZMQ上下文
        context = zmq_ctx_new();
        if (!context) {
            throw std::runtime_error("Failed to create ZMQ context");
        }
        
        // 创建RTSP地址发布者socket (端口5555)
        rtsp_socket = zmq_socket(context, ZMQ_PUB);
        if (!rtsp_socket) {
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to create RTSP socket");
        }
        
        int rc = zmq_bind(rtsp_socket, "tcp://*:5555");
        if (rc != 0) {
            zmq_close(rtsp_socket);
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to bind RTSP socket to port 5555: " + 
                                    std::string(zmq_strerror(zmq_errno())));
        }
        
        // 创建报警信息发布者socket (端口5556)
        alarm_socket = zmq_socket(context, ZMQ_PUB);
        if (!alarm_socket) {
            zmq_close(rtsp_socket);
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to create alarm socket");
        }
        
        rc = zmq_bind(alarm_socket, "tcp://*:5556");
        if (rc != 0) {
            zmq_close(alarm_socket);
            zmq_close(rtsp_socket);
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to bind alarm socket to port 5556: " + 
                                    std::string(zmq_strerror(zmq_errno())));
        }
        
        std::cout << "ZMQ测试服务器初始化成功" << std::endl;
        std::cout << "RTSP地址服务器: tcp://*:5555" << std::endl;
        std::cout << "报警信息服务器: tcp://*:5556" << std::endl;
    }
    
    ~ZmqTestServer() {
        stop();
        if (alarm_socket) zmq_close(alarm_socket);
        if (rtsp_socket) zmq_close(rtsp_socket);
        if (context) zmq_ctx_destroy(context);
        std::cout << "ZMQ测试服务器已关闭" << std::endl;
    }
    
    void start() {
        running = true;
        
        // 启动RTSP地址发送线程
        std::thread rtsp_thread([this]() {
            this->sendRtspAddresses();
        });
        
        // 启动报警信息发送线程
        std::thread alarm_thread([this]() {
            this->sendAlarmMessages();
        });
        
        std::cout << "服务器已启动，按 Enter 键停止..." << std::endl;
        std::cin.get();
        
        stop();
        
        if (rtsp_thread.joinable()) rtsp_thread.join();
        if (alarm_thread.joinable()) alarm_thread.join();
    }
    
    void stop() {
        running = false;
    }

private:
    void sendRtspAddresses() {
        std::cout << "RTSP地址发送线程已启动..." << std::endl;
        
        size_t url_index = 0;
        while (running) {
            const std::string& url = rtsp_urls[url_index];
            
            int rc = zmq_send(rtsp_socket, url.c_str(), url.length(), ZMQ_DONTWAIT);
            if (rc == -1 && zmq_errno() != EAGAIN) {
                std::cerr << "发送RTSP地址失败: " << zmq_strerror(zmq_errno()) << std::endl;
            } else if (rc > 0) {
                std::cout << "[RTSP] 发送: " << url << std::endl;
            }
            
            // 循环切换URL
            url_index = (url_index + 1) % rtsp_urls.size();
            
            // 每3秒发送一次
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        
        std::cout << "RTSP地址发送线程已停止" << std::endl;
    }
    
    void sendAlarmMessages() {
        std::cout << "报警信息发送线程已启动..." << std::endl;
        
        while (running) {
            // 生成随机报警信息
            std::string stream_id = stream_ids[stream_dist(gen)];
            std::string class_name = class_names[class_dist(gen)];
            float confidence = confidence_dist(gen);
            
            std::string alarm_msg = "流ID：" + stream_id + 
                                  " 检测到目标 类别：" + class_name + 
                                  " 置信度：" + std::to_string(confidence) + ";";
            
            int rc = zmq_send(alarm_socket, alarm_msg.c_str(), alarm_msg.length(), ZMQ_DONTWAIT);
            if (rc == -1 && zmq_errno() != EAGAIN) {
                std::cerr << "发送报警信息失败: " << zmq_strerror(zmq_errno()) << std::endl;
            } else if (rc > 0) {
                std::cout << "[ALARM] 发送: " << alarm_msg << std::endl;
            }
            
            // 随机间隔发送 (1-4秒)
            int delay = std::uniform_int_distribution<int>(1, 4)(gen);
            std::this_thread::sleep_for(std::chrono::seconds(delay));
        }
        
        std::cout << "报警信息发送线程已停止" << std::endl;
    }
};

int main() {
    std::cout << "=== ZMQ双服务器测试程序 ===" << std::endl;
    std::cout << "功能说明:" << std::endl;
    std::cout << "1. 端口5555: 循环发送4个RTSP流地址" << std::endl;
    std::cout << "2. 端口5556: 随机发送模拟报警信息" << std::endl;
    std::cout << "===============================" << std::endl;
    
    try {
        ZmqTestServer server;
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }
    
    std::cout << "程序结束" << std::endl;
    return 0;
}
