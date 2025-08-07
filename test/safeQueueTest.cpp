// SafeQueue 使用示例
#include "safeQueue.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== SafeQueue 限制长度测试 ===" << std::endl;
    
    // 创建最大长度为3的安全队列
    SafeQueue<int> queue(3);
    
    std::cout << "创建最大长度为 " << queue.max_size() << " 的队列" << std::endl;
    
    // 测试1：正常添加数据
    std::cout << "\n--- 测试1：正常添加数据 ---" << std::endl;
    for (int i = 1; i <= 3; ++i) {
        bool success = queue.push(i);
        std::cout << "添加数据 " << i << ": " << (success ? "成功" : "失败") 
                  << ", 队列大小: " << queue.size() << std::endl;
    }
    
    // 测试2：队列满后继续添加（数据会被丢弃）
    std::cout << "\n--- 测试2：队列满后继续添加 ---" << std::endl;
    for (int i = 4; i <= 6; ++i) {
        bool success = queue.push(i);
        std::cout << "添加数据 " << i << ": " << (success ? "成功" : "失败（队列已满）") 
                  << ", 队列大小: " << queue.size() 
                  << ", 丢弃数量: " << queue.dropped_count() << std::endl;
    }
    
    // 测试3：从队列中取出数据
    std::cout << "\n--- 测试3：从队列中取出数据 ---" << std::endl;
    while (!queue.empty()) {
        int value;
        if (queue.pop(value)) {
            std::cout << "取出数据: " << value << ", 剩余队列大小: " << queue.size() << std::endl;
        }
    }
    
    // 测试4：队列空后再次添加
    std::cout << "\n--- 测试4：队列空后再次添加 ---" << std::endl;
    for (int i = 7; i <= 9; ++i) {
        bool success = queue.push(i);
        std::cout << "添加数据 " << i << ": " << (success ? "成功" : "失败") 
                  << ", 队列大小: " << queue.size() << std::endl;
    }
    
    std::cout << "\n总丢弃数量: " << queue.dropped_count() << std::endl;
    
    // 测试5：多线程场景
    std::cout << "\n--- 测试5：多线程生产者消费者 ---" << std::endl;
    
    SafeQueue<std::string> str_queue(5); // 最大长度为5的字符串队列
    
    // 生产者线程：快速生产数据
    std::thread producer([&str_queue]() {
        for (int i = 1; i <= 20; ++i) {
            std::string data = "数据" + std::to_string(i);
            bool success = str_queue.push(data);
            std::cout << "生产: " << data << " " << (success ? "成功" : "丢弃") << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 快速生产
        }
    });
    
    // 消费者线程：慢速消费数据
    std::thread consumer([&str_queue]() {
        for (int i = 0; i < 15; ++i) { // 只消费15个，让队列有机会满
            std::string data;
            if (str_queue.pop(data)) {
                std::cout << "消费: " << data << ", 队列大小: " << str_queue.size() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 慢速消费
        }
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "多线程测试完成，总丢弃数量: " << str_queue.dropped_count() << std::endl;
    
    return 0;
}
