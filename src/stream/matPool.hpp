#ifndef MAT_POOL_HPP
#define MAT_POOL_HPP

#include <opencv2/opencv.hpp>
#include <memory>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <string>
#include <atomic>

/**
 * @brief Mat内存池类，用于管理不同尺寸的cv::Mat对象，避免频繁的内存分配和释放
 * 
 * 特性：
 * - 支持多种分辨率的内存池
 * - 线程安全
 * - 自动扩容和回收
 * - 统计功能
 */
class MatPool {
public:
    /**
     * @brief 构造函数
     * @param max_pool_size 每个尺寸池的最大容量
     * @param initial_pool_size 每个尺寸池的初始容量
     */
    MatPool(size_t max_pool_size = 50, size_t initial_pool_size = 10);
    
    /**
     * @brief 析构函数
     */
    ~MatPool();

    /**
     * @brief 获取指定尺寸的Mat对象
     * @param width 图像宽度
     * @param height 图像高度
     * @param type OpenCV Mat类型（默认CV_8UC3）
     * @return shared_ptr<cv::Mat> 返回可用的Mat对象
     */
    std::shared_ptr<cv::Mat> getMat(int width, int height, int type = CV_8UC3);

    /**
     * @brief 返还Mat对象到池中（通过自定义删除器自动调用）
     * @param mat Mat对象指针
     * @param pool_key 池的键值
     */
    void returnMat(cv::Mat* mat, const std::string& pool_key);

    /**
     * @brief 预分配指定尺寸的Mat对象
     * @param width 图像宽度
     * @param height 图像高度
     * @param count 预分配数量
     * @param type OpenCV Mat类型
     */
    void preallocate(int width, int height, int count, int type = CV_8UC3);

    /**
     * @brief 清理指定尺寸的内存池
     * @param width 图像宽度
     * @param height 图像高度
     * @param type OpenCV Mat类型
     */
    void clearPool(int width, int height, int type = CV_8UC3);

    /**
     * @brief 清理所有内存池
     */
    void clearAllPools();

    /**
     * @brief 获取内存池统计信息
     */
    void printStats() const;

    /**
     * @brief 获取指定池的当前大小
     */
    size_t getPoolSize(int width, int height, int type = CV_8UC3) const;

    /**
     * @brief 获取总的池数量
     */
    size_t getTotalPools() const;

private:
    struct PoolInfo {
        std::queue<std::unique_ptr<cv::Mat>> available_mats;
        std::atomic<size_t> total_created{0};
        std::atomic<size_t> total_reused{0};
        std::atomic<size_t> current_in_use{0};
        mutable std::mutex mutex;
    };

    /**
     * @brief 生成池的键值
     */
    std::string generatePoolKey(int width, int height, int type) const;

    /**
     * @brief 创建新的Mat对象
     */
    std::unique_ptr<cv::Mat> createMat(int width, int height, int type) const;

    /**
     * @brief 获取或创建池信息
     */
    PoolInfo& getOrCreatePool(const std::string& key);

    // 成员变量
    mutable std::mutex pools_mutex_;
    std::unordered_map<std::string, std::unique_ptr<PoolInfo>> pools_;
    
    const size_t max_pool_size_;
    const size_t initial_pool_size_;
    
    // 统计信息
    mutable std::atomic<size_t> total_allocations_{0};
    mutable std::atomic<size_t> total_pool_hits_{0};
    mutable std::atomic<size_t> total_pool_misses_{0};
};

#endif // MAT_POOL_HPP
