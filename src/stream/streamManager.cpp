#include "streamManager.hpp"
#include <iostream>
#include <thread>
#include <chrono>

MultiStreamManager::MultiStreamManager(const Config& config) 
    : config_(config), running_(false) {
    printf("多路流管理器初始化完成\n");
}

MultiStreamManager::~MultiStreamManager() {
    stop();
}

bool MultiStreamManager::start() {

    // 初始化流媒体
    mk_config config;
    memset(&config, 0, sizeof(mk_config));
    config.log_mask = LOG_CONSOLE;
    config.thread_num = 4;
    mk_env_init(&config);
    mk_rtsp_server_start(config_.rtsp_server.port, 0);
    
    if (config_.rtsp_server.port > 0){
        //初始化信息服务器
        alarm_server_ = new msgServer(5555, 5556,config_);
        alarm_server_->start(); // 启动报警服务器线程
    }

    printf("=== 启动多路流管理器 ===\n");
    
    auto enabled_streams = config_.getEnabledStreams();
    if (enabled_streams.empty()) {
        printf("警告: 没有启用的流配置\n");
        return false;
    }
    
    printf("检测到 %zu 路启用的流\n", enabled_streams.size());
    
    int success_count = 0;
    for (const auto& stream : enabled_streams) {
        printf("正在启动流: %s (%s)\n", stream.id.c_str(), stream.name.c_str());
        printf("  输入: %s\n", stream.input_url.c_str());
        printf("  输出: rtsp://localhost:%d/%s/%s\n", 
               config_.rtsp_server.port, stream.output_app.c_str(), stream.output_stream.c_str());
        
        try {
            auto worker = std::make_unique<RtspWorker>(
                stream.name,           // stream_name
                stream.input_url,      // stream_url
                config_.rtsp_server.port,  // port
                stream.output_app,     // push_path_first
                stream.output_stream,  // push_path_second
                config_.global.model_root,  // model_root
                config_.global.thread_num,  // thread_num
                this->alarm_server_    // alarm_server
            );
            
            worker->start();
            workers_[stream.id] = std::move(worker);
            success_count++;
            printf("  ✓ 流 %s 启动成功\n", stream.id.c_str());
            
        } catch (const std::exception& e) {
            printf("  ✗ 流 %s 启动失败: %s\n", stream.id.c_str(), e.what());
        }
        
        // 添加短暂延迟，避免同时启动造成资源竞争
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    
    running_ = (success_count > 0);
    printf("=== 多路流管理器启动完成 ===\n");
    printf("成功启动: %d/%zu 路流\n", success_count, enabled_streams.size());
    
    return running_;
}

void MultiStreamManager::stop() {
    if (!running_) {
        printf("多路流管理器未运行\n");
        return;
    }
    
    printf("=== 停止多路流管理器 ===\n");
    
    for (auto& [id, worker] : workers_) {
        printf("正在停止流: %s\n", id.c_str());
        try {
            worker->stop();
            printf("  ✓ 流 %s 已停止\n", id.c_str());
        } catch (const std::exception& e) {
            printf("  ✗ 停止流 %s 时出错: %s\n", id.c_str(), e.what());
        }
    }
    
    workers_.clear();
    running_ = false;
    printf("=== 多路流管理器已停止 ===\n");
}

bool MultiStreamManager::startStream(const std::string& stream_id) {
    if (workers_.find(stream_id) != workers_.end()) {
        printf("流 %s 已经在运行中\n", stream_id.c_str());
        return true;
    }
    
    StreamConfig stream = config_.getStreamById(stream_id);
    if (stream.id.empty()) {
        printf("错误: 未找到流配置 %s\n", stream_id.c_str());
        return false;
    }
    
    printf("启动单个流: %s (%s)\n", stream.id.c_str(), stream.name.c_str());
    
    try {
        auto worker = std::make_unique<RtspWorker>(
            stream.name,           // stream_name
            stream.input_url,      // stream_url
            config_.rtsp_server.port,  // port
            stream.output_app,     // push_path_first
            stream.output_stream,  // push_path_second
            config_.global.model_root,  // model_root
            config_.global.thread_num,  // thread_num
            this->alarm_server_    // alarm_server
        );
        
        worker->start();
        workers_[stream_id] = std::move(worker);
        printf("✓ 流 %s 启动成功\n", stream_id.c_str());
        return true;
        
    } catch (const std::exception& e) {
        printf("✗ 流 %s 启动失败: %s\n", stream_id.c_str(), e.what());
        return false;
    }
}

bool MultiStreamManager::stopStream(const std::string& stream_id) {
    auto it = workers_.find(stream_id);
    if (it == workers_.end()) {
        printf("流 %s 未运行或不存在\n", stream_id.c_str());
        return false;
    }
    
    printf("停止流: %s\n", stream_id.c_str());
    try {
        it->second->stop();
        workers_.erase(it);
        printf("✓ 流 %s 已停止\n", stream_id.c_str());
        return true;
    } catch (const std::exception& e) {
        printf("✗ 停止流 %s 时出错: %s\n", stream_id.c_str(), e.what());
        return false;
    }
}

bool MultiStreamManager::isStreamRunning(const std::string& stream_id) {
    auto it = workers_.find(stream_id);
    if (it == workers_.end()) {
        return false;
    }
    
    // 检查 worker 是否还在运行
    return it->second->isRunning();
}

std::vector<std::string> MultiStreamManager::getRunningStreams() {
    std::vector<std::string> running_streams;
    
    for (const auto& [id, worker] : workers_) {
        if (worker->isRunning()) {
            running_streams.push_back(id);
        }
    }
    
    return running_streams;
}

void MultiStreamManager::showStatus() {
    printf("\n=== 多路流状态信息 ===\n");
    printf("管理器状态: %s\n", running_ ? "运行中" : "已停止");
    printf("RTSP服务器端口: %d\n", config_.rtsp_server.port);
    printf("模型路径: %s\n", config_.global.model_root.c_str());
    printf("推理线程数: %d\n", config_.global.thread_num);
    printf("--------------------------------\n");
    
    if (config_.streams.empty()) {
        printf("没有配置的流\n");
        return;
    }
    
    for (const auto& stream : config_.streams) {
        bool running = isStreamRunning(stream.id);
        bool enabled = stream.enable;
        
        printf("流ID: %s\n", stream.id.c_str());
        printf("  名称: %s\n", stream.name.c_str());
        printf("  状态: %s\n", running ? "🟢 运行中" : (enabled ? "🔴 已停止" : "⚪ 未启用"));
        printf("  输入: %s\n", stream.input_url.c_str());
        printf("  输出: rtsp://localhost:%d/%s/%s\n", 
               config_.rtsp_server.port, stream.output_app.c_str(), stream.output_stream.c_str());
        printf("  启用: %s\n", enabled ? "是" : "否");
        printf("  --------------------------------\n");
    }
    
    auto running_streams = getRunningStreams();
    printf("总计: %zu 路流配置，%zu 路运行中\n", config_.streams.size(), running_streams.size());
}

// 添加其他有用的管理方法
bool MultiStreamManager::restartStream(const std::string& stream_id) {
    printf("重启流: %s\n", stream_id.c_str());
    
    if (isStreamRunning(stream_id)) {
        if (!stopStream(stream_id)) {
            return false;
        }
        // 等待完全停止
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    return startStream(stream_id);
}

void MultiStreamManager::restartAll() {
    printf("重启所有流...\n");
    stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    start();
}

std::vector<StreamConfig> MultiStreamManager::getAllStreams() const {
    return config_.streams;
}

StreamConfig MultiStreamManager::getStreamInfo(const std::string& stream_id) const {
    return config_.getStreamById(stream_id);
}

// 健康检查
void MultiStreamManager::healthCheck() {
    printf("\n=== 流健康检查 ===\n");
    
    for (const auto& [id, worker] : workers_) {
        bool healthy = worker->isRunning();
        printf("流 %s: %s\n", id.c_str(), healthy ? "✓ 正常" : "✗ 异常");
        
        if (!healthy) {
            printf("  尝试重启流 %s...\n", id.c_str());
            // 可以在这里添加自动重启逻辑
        }
    }
}