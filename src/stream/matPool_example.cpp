// matPool_example.cpp - 内存池使用示例

#include "stream/matPool.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

void testBasicUsage() {
    std::cout << "=== 基本使用测试 ===" << std::endl;
    
    MatPool pool(20, 5);  // 最大20个，初始5个
    
    // 预分配一些常见尺寸
    pool.preallocate(640, 480, 3);
    pool.preallocate(1280, 720, 2);
    
    // 获取Mat对象
    auto mat1 = pool.getMat(640, 480);
    auto mat2 = pool.getMat(640, 480);
    auto mat3 = pool.getMat(1280, 720);
    
    std::cout << "获取了3个Mat对象" << std::endl;
    
    // 使用Mat对象
    mat1->setTo(cv::Scalar(255, 0, 0));  // 蓝色
    mat2->setTo(cv::Scalar(0, 255, 0));  // 绿色
    mat3->setTo(cv::Scalar(0, 0, 255));  // 红色
    
    std::cout << "Mat1 尺寸: " << mat1->cols << "x" << mat1->rows << std::endl;
    std::cout << "Mat2 尺寸: " << mat2->cols << "x" << mat2->rows << std::endl;
    std::cout << "Mat3 尺寸: " << mat3->cols << "x" << mat3->rows << std::endl;
    
    // 对象会在离开作用域时自动返回池中
    pool.printStats();
}

void testMultiThreadUsage() {
    std::cout << "\n=== 多线程使用测试 ===" << std::endl;
    
    MatPool pool(100, 20);
    
    // 预分配
    pool.preallocate(1920, 1080, 10);
    pool.preallocate(1280, 720, 10);
    
    std::vector<std::thread> threads;
    
    // 创建多个线程同时使用内存池
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&pool, i]() {
            for (int j = 0; j < 50; ++j) {
                // 模拟不同分辨率的需求
                int width = (j % 2 == 0) ? 1920 : 1280;
                int height = (j % 2 == 0) ? 1080 : 720;
                
                auto mat = pool.getMat(width, height);
                
                // 模拟图像处理
                mat->setTo(cv::Scalar(i * 50, j * 2, (i + j) * 10));
                
                // 模拟处理时间
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                
                if (j % 10 == 0) {
                    std::cout << "线程 " << i << " 处理了 " << (j + 1) << " 帧" << std::endl;
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    pool.printStats();
}

void testPerformanceComparison() {
    std::cout << "\n=== 性能对比测试 ===" << std::endl;
    
    const int iterations = 1000;
    const int width = 1920;
    const int height = 1080;
    
    // 测试直接创建Mat的性能
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        cv::Mat mat(height, width, CV_8UC3);
        mat.setTo(cv::Scalar(i % 255, (i * 2) % 255, (i * 3) % 255));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_direct = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 测试使用内存池的性能
    MatPool pool(50, 20);
    pool.preallocate(width, height, 20);
    
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto mat = pool.getMat(width, height);
        mat->setTo(cv::Scalar(i % 255, (i * 2) % 255, (i * 3) % 255));
    }
    end = std::chrono::high_resolution_clock::now();
    auto duration_pool = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "直接创建Mat用时: " << duration_direct.count() << " ms" << std::endl;
    std::cout << "使用内存池用时: " << duration_pool.count() << " ms" << std::endl;
    std::cout << "性能提升: " << (double)duration_direct.count() / duration_pool.count() << "x" << std::endl;
    
    pool.printStats();
}

int main() {
    try {
        testBasicUsage();
        testMultiThreadUsage();
        testPerformanceComparison();
        
        std::cout << "\n所有测试完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
