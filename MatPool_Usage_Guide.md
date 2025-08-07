# MatPool 内存池使用指南

## 概述

MatPool 是一个专门为 OpenCV Mat 对象设计的内存池类，用于解决多路视频流处理中频繁创建和销毁不同尺寸 Mat 对象导致的内存碎片化问题。

## 主要特性

- **多尺寸支持**: 自动管理不同分辨率的 Mat 对象池
- **线程安全**: 支持多线程并发访问
- **自动回收**: 使用智能指针自动管理 Mat 生命周期
- **统计功能**: 提供详细的使用统计信息
- **内存控制**: 可配置池大小，防止内存无限增长

## 解决的问题

### 原问题分析
```cpp
// 原始代码中的问题
cv::Mat origin_mat = cv::Mat::zeros(height, width, CV_8UC3);  // 每帧都创建新的Mat
```

当处理多路不同分辨率的视频流时：
- **1080p流**: 1920×1080×3 = 6,220,800 字节
- **720p流**: 1280×720×3 = 2,764,800 字节
- **480p流**: 640×480×3 = 921,600 字节

频繁的分配和释放不同大小的内存块导致内存碎片化，最终引发堆损坏。

### 解决方案
使用内存池预分配和重用 Mat 对象，避免频繁的内存操作。

## 基本使用方法

### 1. 初始化内存池

```cpp
#include "stream/matPool.hpp"

// 创建内存池：最大50个对象，初始预分配10个
MatPool mat_pool(50, 10);

// 预分配常见分辨率
mat_pool.preallocate(1920, 1080, 5);  // 1080p，预分配5个
mat_pool.preallocate(1280, 720, 5);   // 720p，预分配5个
mat_pool.preallocate(640, 480, 3);    // 480p，预分配3个
```

### 2. 在视频处理中使用

```cpp
// 原始代码（有内存碎片问题）
cv::Mat origin_mat = cv::Mat::zeros(height, width, CV_8UC3);

// 使用内存池的代码
std::shared_ptr<cv::Mat> origin_mat = mat_pool.getMat(width, height, CV_8UC3);
```

### 3. 自动回收

```cpp
{
    auto mat = mat_pool.getMat(1920, 1080);
    // 使用 mat 进行图像处理
    mat->setTo(cv::Scalar(0, 255, 0));
    // ...
} // mat 自动返回池中，无需手动释放
```

## 在 StreamHive 中的集成

### 1. 上下文结构修改

在 `av_worker_context_t` 中添加内存池：
```cpp
typedef struct {
    // ... 其他成员
    framePool *pool;      // 线程池对象
    MatPool *mat_pool;    // Mat内存池对象
} av_worker_context_t;
```

### 2. 初始化（worker.cpp）

```cpp
// 在 RtspWorker 构造函数中
ctx_->mat_pool = new MatPool(50, 10);

// 预分配常见分辨率
ctx_->mat_pool->preallocate(1920, 1080, 5);
ctx_->mat_pool->preallocate(1280, 720, 5);
ctx_->mat_pool->preallocate(640, 480, 5);
```

### 3. 使用（avPullStream.cpp）

```cpp
void mpp_decoder_frame_callback(void *userdata, int width_stride, int height_stride, 
                               int width, int height, int format, int fd, void *data) {
    av_worker_context_t *ctx = (av_worker_context_t *)userdata;
    
    // 从内存池获取 Mat 对象
    std::shared_ptr<cv::Mat> origin_mat;
    if (ctx->mat_pool != nullptr) {
        try {
            origin_mat = ctx->mat_pool->getMat(width, height, CV_8UC3);
        } catch (const std::exception& e) {
            printf("Failed to get Mat from pool: %s\n", e.what());
            // 降级到直接创建
            origin_mat = std::make_shared<cv::Mat>(height, width, CV_8UC3);
        }
    } else {
        origin_mat = std::make_shared<cv::Mat>(height, width, CV_8UC3);
    }
    
    // RGA 转换
    rga_buffer_t rgb_img = wrapbuffer_virtualaddr(
        (void *)origin_mat->data, width, height, RK_FORMAT_RGB_888);
    
    // 提交到推理线程池
    ctx->pool->inferenceThread(origin_mat);
}
```

### 4. 清理（worker.cpp）

```cpp
// 在析构函数中
if (ctx_->mat_pool) {
    ctx_->mat_pool->printStats();  // 打印统计信息
    delete ctx_->mat_pool;
    ctx_->mat_pool = nullptr;
}
```

## 配置建议

### 内存池大小配置

```cpp
// 根据同时处理的视频流数量和帧率调整
int max_concurrent_streams = 4;  // 最大同时处理的视频流数量
int fps = 30;                    // 帧率
int processing_delay_frames = 5; // 处理延迟帧数

int pool_size = max_concurrent_streams * processing_delay_frames;
MatPool mat_pool(pool_size * 2, pool_size);  // 留一些余量
```

### 预分配策略

```cpp
// 根据实际使用的分辨率预分配
struct ResolutionInfo {
    int width, height;
    int expected_streams;  // 预期同时使用此分辨率的流数量
};

std::vector<ResolutionInfo> resolutions = {
    {1920, 1080, 2},  // 2路1080p
    {1280, 720,  2},  // 2路720p
    {640,  480,  1}   // 1路480p
};

for (const auto& res : resolutions) {
    mat_pool.preallocate(res.width, res.height, res.expected_streams * 3);
}
```

## 性能优化建议

### 1. 合理的池大小
- 太小：频繁创建新对象，失去池化优势
- 太大：占用过多内存

### 2. 预分配策略
- 启动时预分配常用分辨率
- 根据实际业务场景调整预分配数量

### 3. 监控和调优
```cpp
// 定期打印统计信息
void printPoolStats() {
    mat_pool.printStats();
    
    // 根据命中率调整策略
    // 命中率低于80%时考虑增加预分配数量
}
```

## 故障排除

### 1. 内存不足
```cpp
try {
    auto mat = mat_pool.getMat(width, height);
} catch (const std::bad_alloc& e) {
    // 内存不足，尝试清理或降级处理
    mat_pool.clearAllPools();  // 紧急情况下清理所有池
    // 或者降低分辨率处理
}
```

### 2. 性能问题
- 检查命中率：`mat_pool.printStats()`
- 调整池大小和预分配策略
- 考虑为不同分辨率创建独立的池

### 3. 内存泄漏检查
- 检查 `current_in_use` 数量是否持续增长
- 确保所有 shared_ptr 都能正确释放

## 编译和构建

确保在 CMakeLists.txt 中包含内存池源文件：
```cmake
add_library(stream SHARED
    src/stream/avPullStream.cpp
    src/stream/avPushStream.cpp
    src/RtspWorker/worker.cpp
    src/stream/streamManager.cpp
    src/stream/matPool.cpp  # 添加这一行
)
```

## 总结

MatPool 内存池通过以下方式解决内存碎片问题：

1. **预分配**: 启动时为常见分辨率预分配内存
2. **重用**: 避免频繁的 malloc/free 操作
3. **统一管理**: 自动处理不同尺寸的内存需求
4. **线程安全**: 支持多路视频流并发处理

使用内存池后，你的多路视频流处理应该能够稳定运行，不再出现堆损坏的问题。
