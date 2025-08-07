#ifndef SAFE_MEMORY_HPP
#define SAFE_MEMORY_HPP

#include <memory>
#include <cstdlib>
#include <cstring>

/**
 * @brief 安全的对齐内存分配器，用于避免内存对齐问题
 */
class SafeAlignedMemory {
public:
    /**
     * @brief 分配对齐内存
     * @param size 需要分配的大小
     * @param alignment 对齐字节数（必须是2的幂）
     * @return 分配的内存指针，失败返回nullptr
     */
    static void* allocate(size_t size, size_t alignment = 64) {
        if (size == 0 || (alignment & (alignment - 1)) != 0) {
            return nullptr;  // 无效参数
        }
        
        // 确保大小是对齐值的倍数
        size_t aligned_size = ((size + alignment - 1) / alignment) * alignment;
        
        void* ptr = aligned_alloc(alignment, aligned_size);
        if (ptr) {
            // 清零内存
            memset(ptr, 0, aligned_size);
        }
        
        return ptr;
    }
    
    /**
     * @brief 释放内存
     * @param ptr 要释放的内存指针
     */
    static void deallocate(void* ptr) {
        if (ptr) {
            free(ptr);
        }
    }
};

/**
 * @brief RAII风格的对齐内存管理器
 */
class AlignedBuffer {
public:
    AlignedBuffer(size_t size, size_t alignment = 64) 
        : size_(0), data_(nullptr) {
        data_ = SafeAlignedMemory::allocate(size, alignment);
        if (data_) {
            size_ = ((size + alignment - 1) / alignment) * alignment;
        }
    }
    
    ~AlignedBuffer() {
        SafeAlignedMemory::deallocate(data_);
    }
    
    // 禁止拷贝
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
    
    // 支持移动
    AlignedBuffer(AlignedBuffer&& other) noexcept 
        : size_(other.size_), data_(other.data_) {
        other.size_ = 0;
        other.data_ = nullptr;
    }
    
    AlignedBuffer& operator=(AlignedBuffer&& other) noexcept {
        if (this != &other) {
            SafeAlignedMemory::deallocate(data_);
            size_ = other.size_;
            data_ = other.data_;
            other.size_ = 0;
            other.data_ = nullptr;
        }
        return *this;
    }
    
    void* data() const { return data_; }
    size_t size() const { return size_; }
    bool valid() const { return data_ != nullptr; }
    
    char* as_char() const { return static_cast<char*>(data_); }
    
private:
    size_t size_;
    void* data_;
};

#endif // SAFE_MEMORY_HPP
