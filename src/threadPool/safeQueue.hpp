#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class SafeQueue {
public:
    // 构造函数，可以指定最大队列长度，默认为0表示无限制
    explicit SafeQueue(size_t max_size = 0) : max_size_(max_size) {}
    
    // 返回是否成功添加到队列
    bool push(const T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        // 如果设置了最大队列长度且队列已满，则丢弃新数据
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            dropped_count_++;
            return false; // 数据被丢弃
        }
        
        queue_.push(value);
        cv_.notify_one();
        return true; // 数据成功添加
    }
    
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]{ return !queue_.empty(); });
        value = queue_.front();
        queue_.pop();
        // printf("queue size: %zu\n", queue_.size());
        return true;
    }
    
    size_t size() {
        std::unique_lock<std::mutex> lock(mtx_);
        return queue_.size();
    }
    
    // 获取最大队列长度
    size_t max_size() const {
        return max_size_;
    }
    
    // 获取被丢弃的数据数量
    size_t dropped_count() const {
        std::unique_lock<std::mutex> lock(mtx_);
        return dropped_count_;
    }
    
    // 重置丢弃计数器
    void reset_dropped_count() {
        std::unique_lock<std::mutex> lock(mtx_);
        dropped_count_ = 0;
    }
    
    // 检查队列是否为空
    bool empty() {
        std::unique_lock<std::mutex> lock(mtx_);
        return queue_.empty();
    }
    
    // 检查队列是否已满
    bool full() {
        std::unique_lock<std::mutex> lock(mtx_);
        return max_size_ > 0 && queue_.size() >= max_size_;
    }
    
    // 清空队列
    void clear() {
        std::unique_lock<std::mutex> lock(mtx_);
        std::queue<T> empty_queue;
        queue_.swap(empty_queue);
    }
    
private:
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    size_t max_size_;          // 最大队列长度，0表示无限制
    size_t dropped_count_ = 0; // 被丢弃的数据数量
};