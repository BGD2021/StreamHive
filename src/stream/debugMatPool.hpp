#ifndef DEBUG_MAT_POOL_HPP
#define DEBUG_MAT_POOL_HPP

#include "matPool.hpp"
#include <unordered_set>
#include <atomic>

/**
 * @brief 调试版本的Mat内存池，用于跟踪内存使用情况
 */
class DebugMatPool : public MatPool {
public:
    DebugMatPool(size_t max_pool_size = 50, size_t initial_pool_size = 10);
    ~DebugMatPool();
    
    std::shared_ptr<cv::Mat> getMat(int width, int height, int type = CV_8UC3) override;
    void returnMat(cv::Mat* mat, const std::string& pool_key) override;
    
    // 调试信息
    void printDetailedStats() const;
    size_t getActiveMatCount() const;
    bool hasMemoryLeaks() const;
    
private:
    mutable std::mutex debug_mutex_;
    std::unordered_set<cv::Mat*> active_mats_;
    std::atomic<size_t> total_gets_{0};
    std::atomic<size_t> total_returns_{0};
};

#endif // DEBUG_MAT_POOL_HPP
