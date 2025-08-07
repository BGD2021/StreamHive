// 创建一个简单的ZMQ订阅者来测试
#include <zmq.h>
#include <iostream>
#include <string>

int main() {
    void *context = zmq_ctx_new();
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    
    // 连接到你的板子
    zmq_connect(subscriber, "tcp://localhost:5556");
    
    // 订阅所有消息
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    
    while (true) {
        char buffer[256];
        int size = zmq_recv(subscriber, buffer, 255, 0);
        if (size > 0) {
            buffer[size] = '\0';
            std::cout << "Received: " << buffer << std::endl;
        }
    }
    
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    return 0;
}