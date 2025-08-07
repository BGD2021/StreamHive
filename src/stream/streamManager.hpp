#pragma once
#include <vector>
#include <memory>
#include <map>
#include <string>
#include "config/config.hpp"
#include "RtspWorker/worker.hpp"
#include "utils/msgServer.hpp"

class MultiStreamManager {
public:
    MultiStreamManager(const Config& config);
    ~MultiStreamManager();
    
    // 基础流管理
    bool start();
    void stop();
    
    // 单个流管理
    bool startStream(const std::string& stream_id);
    bool stopStream(const std::string& stream_id);
    bool restartStream(const std::string& stream_id);
    bool isStreamRunning(const std::string& stream_id);
    
    // 批量操作
    void restartAll();
    
    // 状态查询
    std::vector<std::string> getRunningStreams();
    std::vector<StreamConfig> getAllStreams() const;
    StreamConfig getStreamInfo(const std::string& stream_id) const;
    void showStatus();
    
    // 健康检查
    void healthCheck();
    
    // 管理器状态
    bool isRunning() const { return running_; }
    size_t getStreamCount() const { return config_.streams.size(); }
    size_t getRunningStreamCount() const { return workers_.size(); }
    
private:
    Config config_;
    std::map<std::string, std::unique_ptr<RtspWorker>> workers_;
    bool running_;
    msgServer *alarm_server_; // 消息服务器，用于发送RTSP地址和报警信息
};