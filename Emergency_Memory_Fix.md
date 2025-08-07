# 紧急内存修复方案 - 临时解决方案

## 问题分析

`malloc(): unaligned tcache chunk detected` 错误在1分钟内出现，说明问题比预期更严重。可能的原因：

1. **智能指针循环引用**：shared_ptr 在多线程环境下可能出现问题
2. **OpenCV Mat 内存管理冲突**：与系统内存分配器不兼容  
3. **第三方库内存冲突**：RGA、MPP等库的内存管理问题
4. **编译器优化问题**：可能导致内存对齐异常

## 立即修复措施

### 1. 禁用所有复杂内存管理

已经实施的修复：
- ✅ 完全禁用Mat内存池
- ✅ 使用简单的malloc/free替代对齐内存分配
- ✅ 使用cv::Mat::zeros确保正确初始化

### 2. 添加运行时检查

在关键位置添加内存检查：

```bash
# 运行时启用内存检查
export MALLOC_CHECK_=2
export MALLOC_PERTURB_=17
./StreamHive [config]
```

### 3. 编译时调试选项

修改编译命令，添加调试信息：

```bash
# 清理重新编译
rm -rf build/*
cd build

# 使用调试模式编译
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-g -O0 -fno-omit-frame-pointer" \
      ..
make

# 如果仍有问题，尝试AddressSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=address -fno-omit-frame-pointer" \
      ..
make
```

### 4. 运行时监控

```bash
# 使用valgrind检测内存问题
valgrind --tool=memcheck \
         --leak-check=full \
         --track-origins=yes \
         --show-leak-kinds=all \
         ./StreamHive [config]

# 如果valgrind太慢，使用轻量级检查
./StreamHive [config] 2>&1 | tee debug.log
```

## 进一步诊断

如果问题仍然存在，请提供以下信息：

### 1. 系统信息
```bash
# 系统版本
cat /etc/os-release
uname -a

# 内存信息
free -h
cat /proc/meminfo | head -10

# 编译器版本
gcc --version
```

### 2. 依赖库版本
```bash
# OpenCV版本
pkg-config --modversion opencv4

# 检查链接的库
ldd ./StreamHive
```

### 3. 错误详情
- 具体的错误堆栈（如果有）
- 错误发生的时间点
- 处理的视频流数量和分辨率

## 临时降级方案

如果修复仍不成功，可以尝试以下降级方案：

### 方案A：单线程处理
```cpp
// 在worker.cpp中
ctx_->pool = new framePool(model_file, 1);  // 只使用1个线程
```

### 方案B：减少内存使用
```cpp
// 在avPullStream.cpp中
// 直接在推理前缩放图像
cv::Mat small_mat;
cv::resize(*origin_mat, small_mat, cv::Size(640, 480));
ctx->pool->inferenceThread(std::make_shared<cv::Mat>(small_mat));
```

### 方案C：增加延迟降低负载
```cpp
// 在推理线程中添加延迟
std::this_thread::sleep_for(std::chrono::milliseconds(10));
```

## 检查清单

在运行修复版本之前，请确认：

- [ ] 已经重新编译整个项目
- [ ] 已经清理旧的build文件
- [ ] 系统内存充足（>2GB可用）
- [ ] 没有其他占用内存的进程
- [ ] 使用的是Debug模式编译

## 预期结果

这个修复应该：
1. 完全避免内存池相关的对齐问题
2. 使用最基本的内存分配方式
3. 提供详细的错误信息用于进一步诊断

如果这个修复仍然不能解决问题，那么问题可能来自：
- 第三方库（OpenCV, RGA, MPP）
- 系统级别的内存管理问题
- 硬件相关的内存对齐要求

请按照上述步骤重新编译并测试，然后报告结果。
