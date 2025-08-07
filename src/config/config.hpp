#ifndef _CONFIG_HPP_
#define _CONFIG_HPP_

#include <string>
#include <vector>

// 流配置结构
struct StreamConfig {
    std::string id;
    std::string name;
    std::string input_url;
    std::string output_app;
    std::string output_stream;
    bool enable = true;
};

// 全局配置结构
struct GlobalConfig {
    std::string model_root;
    int thread_num = 4;
};

// RTSP服务器配置结构
struct RtspServerConfig {
    int port = 3554;
};

class Config {
    public:
        Config(const char*file);
        ~Config();
        void show();
        
        // 获取流配置相关方法
        std::vector<StreamConfig> getEnabledStreams() const;
        StreamConfig getStreamById(const std::string& id) const;
        
    public:
        bool mState=false;
        const char* file = NULL;
        
        // 新的配置结构
        GlobalConfig global;
        RtspServerConfig rtsp_server;
        std::vector<StreamConfig> streams;
        
        // 向后兼容的成员变量
        int thread_num;
        int port;
        std::string camera_rtsp; // 摄像头流地址
        std::string model_root;//模型地址
        std::string push_first;
        std::string push_second;
};


#endif