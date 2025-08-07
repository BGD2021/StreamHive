#include "matPool.hpp"
#include <iostream>
#include <sstream>

MatPool::MatPool(size_t max_pool_size, size_t initial_pool_size)
    : max_pool_size_(max_pool_size), initial_pool_size_(initial_pool_size) {
    std::cout << "MatPool initialized with max_size=" << max_pool_size 
              << ", initial_size=" << initial_pool_size << std::endl;
}

MatPool::~MatPool() {
    clearAllPools();
    std::cout << "MatPool destroyed" << std::endl;
}

std::string MatPool::generatePoolKey(int width, int height, int type) const {
    return std::to_string(width) + "x" + std::to_string(height) + "_" + std::to_string(type);
}

std::unique_ptr<cv::Mat> MatPool::createMat(int width, int height, int type) const {
    try {
        auto mat = std::make_unique<cv::Mat>(height, width, type);
        if (mat->empty() || mat->data == nullptr) {
            throw std::runtime_error("Failed to create Mat with valid data");
        }
        return mat;
    } catch (const std::exception& e) {
        std::cerr << "Error creating Mat(" << width << "x" << height << ", type=" << type 
                  << "): " << e.what() << std::endl;
        throw;
    }
}

MatPool::PoolInfo& MatPool::getOrCreatePool(const std::string& key) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = pools_.find(key);
    if (it == pools_.end()) {
        pools_[key] = std::make_unique<PoolInfo>();
        std::cout << "Created new pool for key: " << key << std::endl;
    }
    
    return *pools_[key];
}

std::shared_ptr<cv::Mat> MatPool::getMat(int width, int height, int type) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Invalid dimensions: width=" + std::to_string(width) + 
                                  ", height=" + std::to_string(height));
    }

    const std::string pool_key = generatePoolKey(width, height, type);
    PoolInfo& pool = getOrCreatePool(pool_key);
    
    total_allocations_++;
    
    // 尝试从池中获取可用的Mat
    {
        std::lock_guard<std::mutex> lock(pool.mutex);
        if (!pool.available_mats.empty()) {
            auto mat_ptr = std::move(pool.available_mats.front());
            pool.available_mats.pop();
            pool.total_reused++;
            pool.current_in_use++;
            total_pool_hits_++;
            
            // 验证Mat的有效性
            if (mat_ptr && !mat_ptr->empty() && mat_ptr->data != nullptr) {
                // 清零数据（可选，根据需求决定）
                mat_ptr->setTo(cv::Scalar::all(0));
                
                // 使用自定义删除器，确保Mat使用完毕后返回池中
                return std::shared_ptr<cv::Mat>(
                    mat_ptr.release(),
                    [this, pool_key](cv::Mat* mat) {
                        this->returnMat(mat, pool_key);
                    }
                );
            }
        }
    }
    
    // 池中没有可用的Mat，创建新的
    total_pool_misses_++;
    auto new_mat = createMat(width, height, type);
    
    {
        std::lock_guard<std::mutex> lock(pool.mutex);
        pool.total_created++;
        pool.current_in_use++;
    }
    
    // 使用自定义删除器
    return std::shared_ptr<cv::Mat>(
        new_mat.release(),
        [this, pool_key](cv::Mat* mat) {
            this->returnMat(mat, pool_key);
        }
    );
}

void MatPool::returnMat(cv::Mat* mat, const std::string& pool_key) {
    if (!mat || mat->empty() || mat->data == nullptr) {
        delete mat;  // 直接删除无效的Mat
        return;
    }
    
    try {
        PoolInfo& pool = getOrCreatePool(pool_key);
        std::lock_guard<std::mutex> lock(pool.mutex);
        
        pool.current_in_use--;
        
        // 如果池未满，则将Mat返回池中
        if (pool.available_mats.size() < max_pool_size_) {
            pool.available_mats.push(std::unique_ptr<cv::Mat>(mat));
        } else {
            // 池已满，直接删除
            delete mat;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error returning Mat to pool: " << e.what() << std::endl;
        delete mat;  // 发生错误时直接删除
    }
}

void MatPool::preallocate(int width, int height, int count, int type) {
    if (width <= 0 || height <= 0 || count <= 0) {
        throw std::invalid_argument("Invalid parameters for preallocation");
    }
    
    const std::string pool_key = generatePoolKey(width, height, type);
    PoolInfo& pool = getOrCreatePool(pool_key);
    
    std::lock_guard<std::mutex> lock(pool.mutex);
    
    std::cout << "Preallocating " << count << " Mats for " << pool_key << std::endl;
    
    for (int i = 0; i < count && pool.available_mats.size() < max_pool_size_; ++i) {
        try {
            auto mat = createMat(width, height, type);
            pool.available_mats.push(std::move(mat));
            pool.total_created++;
        } catch (const std::exception& e) {
            std::cerr << "Failed to preallocate Mat " << i << ": " << e.what() << std::endl;
            break;
        }
    }
    
    std::cout << "Preallocated " << pool.available_mats.size() << " Mats for " << pool_key << std::endl;
}

void MatPool::clearPool(int width, int height, int type) {
    const std::string pool_key = generatePoolKey(width, height, type);
    
    std::lock_guard<std::mutex> pools_lock(pools_mutex_);
    auto it = pools_.find(pool_key);
    if (it != pools_.end()) {
        PoolInfo& pool = *it->second;
        std::lock_guard<std::mutex> pool_lock(pool.mutex);
        
        size_t cleared_count = pool.available_mats.size();
        while (!pool.available_mats.empty()) {
            pool.available_mats.pop();
        }
        
        std::cout << "Cleared " << cleared_count << " Mats from pool " << pool_key << std::endl;
    }
}

void MatPool::clearAllPools() {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    size_t total_cleared = 0;
    for (auto& pair : pools_) {
        PoolInfo& pool = *pair.second;
        std::lock_guard<std::mutex> pool_lock(pool.mutex);
        
        size_t pool_size = pool.available_mats.size();
        while (!pool.available_mats.empty()) {
            pool.available_mats.pop();
        }
        total_cleared += pool_size;
    }
    
    pools_.clear();
    std::cout << "Cleared all pools, total " << total_cleared << " Mats freed" << std::endl;
}

void MatPool::printStats() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    std::cout << "\n=== MatPool Statistics ===" << std::endl;
    std::cout << "Total allocations: " << total_allocations_ << std::endl;
    std::cout << "Pool hits: " << total_pool_hits_ << std::endl;
    std::cout << "Pool misses: " << total_pool_misses_ << std::endl;
    
    if (total_allocations_ > 0) {
        double hit_rate = (double)total_pool_hits_ / total_allocations_ * 100.0;
        std::cout << "Hit rate: " << hit_rate << "%" << std::endl;
    }
    
    std::cout << "Active pools: " << pools_.size() << std::endl;
    
    for (const auto& pair : pools_) {
        const PoolInfo& pool = *pair.second;
        std::lock_guard<std::mutex> pool_lock(pool.mutex);
        
        std::cout << "  Pool " << pair.first << ":" << std::endl;
        std::cout << "    Available: " << pool.available_mats.size() << std::endl;
        std::cout << "    Total created: " << pool.total_created << std::endl;
        std::cout << "    Total reused: " << pool.total_reused << std::endl;
        std::cout << "    Currently in use: " << pool.current_in_use << std::endl;
    }
    std::cout << "========================\n" << std::endl;
}

size_t MatPool::getPoolSize(int width, int height, int type) const {
    const std::string pool_key = generatePoolKey(width, height, type);
    
    std::lock_guard<std::mutex> pools_lock(pools_mutex_);
    auto it = pools_.find(pool_key);
    if (it != pools_.end()) {
        const PoolInfo& pool = *it->second;
        std::lock_guard<std::mutex> pool_lock(pool.mutex);
        return pool.available_mats.size();
    }
    return 0;
}

size_t MatPool::getTotalPools() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    return pools_.size();
}
