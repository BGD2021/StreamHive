#include <zmq.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

class ZmqTestClient {
private:
    void* context;
    void* rtsp_subscriber;    // 订阅5555端口的RTSP地址
    void* alarm_subscriber;   // 订阅5556端口的报警信息
    std::atomic<bool> running{false};

public:
    ZmqTestClient(const std::string& server_ip = "localhost") {
        // 初始化ZMQ上下文
        context = zmq_ctx_new();
        if (!context) {
            throw std::runtime_error("Failed to create ZMQ context");
        }
        
        // 创建RTSP地址订阅者
        rtsp_subscriber = zmq_socket(context, ZMQ_SUB);
        if (!rtsp_subscriber) {
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to create RTSP subscriber socket");
        }
        
        std::string rtsp_endpoint = "tcp://" + server_ip + ":5555";
        int rc = zmq_connect(rtsp_subscriber, rtsp_endpoint.c_str());
        if (rc != 0) {
            zmq_close(rtsp_subscriber);
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to connect to RTSP server: " + 
                                    std::string(zmq_strerror(zmq_errno())));
        }
        
        // 订阅所有RTSP消息
        zmq_setsockopt(rtsp_subscriber, ZMQ_SUBSCRIBE, "", 0);
        
        // 创建报警信息订阅者
        alarm_subscriber = zmq_socket(context, ZMQ_SUB);
        if (!alarm_subscriber) {
            zmq_close(rtsp_subscriber);
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to create alarm subscriber socket");
        }
        
        std::string alarm_endpoint = "tcp://" + server_ip + ":5556";
        rc = zmq_connect(alarm_subscriber, alarm_endpoint.c_str());
        if (rc != 0) {
            zmq_close(alarm_subscriber);
            zmq_close(rtsp_subscriber);
            zmq_ctx_destroy(context);
            throw std::runtime_error("Failed to connect to alarm server: " + 
                                    std::string(zmq_strerror(zmq_errno())));
        }
        
        // 订阅所有报警消息
        zmq_setsockopt(alarm_subscriber, ZMQ_SUBSCRIBE, "", 0);
        
        // 设置接收超时
        int timeout = 1000; // 1秒超时
        zmq_setsockopt(rtsp_subscriber, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        zmq_setsockopt(alarm_subscriber, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        
        std::cout << "ZMQ测试客户端初始化成功" << std::endl;
        std::cout << "连接到RTSP服务器: " << rtsp_endpoint << std::endl;
        std::cout << "连接到报警服务器: " << alarm_endpoint << std::endl;
    }
    
    ~ZmqTestClient() {
        stop();
        if (alarm_subscriber) zmq_close(alarm_subscriber);
        if (rtsp_subscriber) zmq_close(rtsp_subscriber);
        if (context) zmq_ctx_destroy(context);
        std::cout << "ZMQ测试客户端已关闭" << std::endl;
    }
    
    void start() {
        running = true;
        
        // 启动RTSP地址接收线程
        std::thread rtsp_thread([this]() {
            this->receiveRtspAddresses();
        });
        
        // 启动报警信息接收线程
        std::thread alarm_thread([this]() {
            this->receiveAlarmMessages();
        });
        
        std::cout << "客户端已启动，按 Enter 键停止..." << std::endl;
        std::cin.get();
        
        stop();
        
        if (rtsp_thread.joinable()) rtsp_thread.join();
        if (alarm_thread.joinable()) alarm_thread.join();
    }
    
    void stop() {
        running = false;
    }

private:
    void receiveRtspAddresses() {
        std::cout << "RTSP地址接收线程已启动..." << std::endl;
        
        int rtsp_count = 0;
        while (running) {
            char buffer[1024];
            int size = zmq_recv(rtsp_subscriber, buffer, sizeof(buffer) - 1, 0);
            
            if (size == -1) {
                if (zmq_errno() == EAGAIN) {
                    // 超时，继续循环
                    continue;
                } else {
                    std::cerr << "接收RTSP地址失败: " << zmq_strerror(zmq_errno()) << std::endl;
                    break;
                }
            }
            
            if (size > 0) {
                buffer[size] = '\0';
                rtsp_count++;
                std::cout << "[RTSP #" << rtsp_count << "] 接收到: " << buffer << std::endl;
            }
        }
        
        std::cout << "RTSP地址接收线程已停止，共接收 " << rtsp_count << " 条消息" << std::endl;
    }
    
    void receiveAlarmMessages() {
        std::cout << "报警信息接收线程已启动..." << std::endl;
        
        int alarm_count = 0;
        while (running) {
            char buffer[1024];
            int size = zmq_recv(alarm_subscriber, buffer, sizeof(buffer) - 1, 0);
            
            if (size == -1) {
                if (zmq_errno() == EAGAIN) {
                    // 超时，继续循环
                    continue;
                } else {
                    std::cerr << "接收报警信息失败: " << zmq_strerror(zmq_errno()) << std::endl;
                    break;
                }
            }
            
            if (size > 0) {
                buffer[size] = '\0';
                alarm_count++;
                std::cout << "[ALARM #" << alarm_count << "] 接收到: " << buffer << std::endl;
            }
        }
        
        std::cout << "报警信息接收线程已停止，共接收 " << alarm_count << " 条消息" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== ZMQ双客户端测试程序 ===" << std::endl;
    
    std::string server_ip = "localhost";
    if (argc > 1) {
        server_ip = argv[1];
        std::cout << "连接到服务器: " << server_ip << std::endl;
    } else {
        std::cout << "使用默认服务器: localhost" << std::endl;
        std::cout << "如需连接其他服务器，请使用: " << argv[0] << " <server_ip>" << std::endl;
    }
    
    std::cout << "===============================" << std::endl;
    
    try {
        ZmqTestClient client(server_ip);
        client.start();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }
    
    std::cout << "程序结束" << std::endl;
    return 0;
}
