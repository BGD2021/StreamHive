#include "config.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <json/json.h>

Config::Config(const char *file) : file(file)
{
    std::ifstream ifs(file, std::ios::binary);

    if (!ifs.is_open())
    {
        printf("open %s error\n", file);
        return;
    }
    else
    {
        Json::CharReaderBuilder builder;
        builder["collectComments"] = true;
        JSONCPP_STRING errs;
        Json::Value root;

        if (parseFromStream(builder, ifs, &root, &errs))
        {
            // 解析全局配置
            if (root.isMember("global") && root["global"].isObject())
            {
                const Json::Value& globalObj = root["global"];
                global.model_root = globalObj.get("model_root", "").asString();
                global.thread_num = globalObj.get("thread_num", 4).asInt();
            }
            
            // 解析RTSP服务器配置
            if (root.isMember("rtsp_server") && root["rtsp_server"].isObject())
            {
                const Json::Value& rtspObj = root["rtsp_server"];
                rtsp_server.port = rtspObj.get("port", 3554).asInt();
            }
            
            // 解析流配置
            if (root.isMember("streams") && root["streams"].isArray())
            {
                const Json::Value& streamsArray = root["streams"];
                for (Json::ArrayIndex i = 0; i < streamsArray.size(); ++i)
                {
                    const Json::Value& streamObj = streamsArray[i];
                    StreamConfig stream;
                    
                    stream.id = streamObj.get("id", "").asString();
                    stream.name = streamObj.get("name", "").asString();
                    stream.input_url = streamObj.get("input_url", "").asString();
                    stream.enable = streamObj.get("enable", true).asBool();
                    
                    // 解析输出配置
                    if (streamObj.isMember("output") && streamObj["output"].isObject())
                    {
                        const Json::Value& outputObj = streamObj["output"];
                        stream.output_app = outputObj.get("app", "live").asString();
                        stream.output_stream = outputObj.get("stream", "test").asString();
                    }
                    else
                    {
                        // 默认值
                        stream.output_app = "live";
                        stream.output_stream = "test";
                    }
                    
                    streams.push_back(stream);
                }
            }
            
            // 向后兼容：设置旧的成员变量
            // 使用全局配置
            model_root = global.model_root;
            thread_num = global.thread_num;
            port = rtsp_server.port;
            
            // 使用第一个流的配置作为默认值
            if (!streams.empty())
            {
                camera_rtsp = streams[0].input_url;
                push_first = streams[0].output_app;
                push_second = streams[0].output_stream;
            }
            else
            {
                // 如果没有配置流，尝试从旧格式读取
                if (root.isMember("camera_rtsp"))
                {
                    camera_rtsp = root["camera_rtsp"].asString();
                    push_first = root.get("push_first", "live").asString();
                    push_second = root.get("push_second", "test").asString();
                    
                    // 创建一个默认流配置
                    StreamConfig defaultStream;
                    defaultStream.id = "default_stream";
                    defaultStream.name = "默认流";
                    defaultStream.input_url = camera_rtsp;
                    defaultStream.output_app = push_first;
                    defaultStream.output_stream = push_second;
                    defaultStream.enable = true;
                    streams.push_back(defaultStream);
                }
            }

            mState = true;
        }
        else
        {
            printf("parse %s error: %s\n", file, errs.c_str());
        }
        ifs.close();
    }
}

Config::~Config()
{
}

void Config::show()
{
    printf("=== StreamHive 配置信息 ===\n");
    printf("配置文件: %s\n", file);
    
    printf("全局配置:\n");
    printf("  模型路径: %s\n", global.model_root.c_str());
    printf("  线程数: %d\n", global.thread_num);
    
    printf("RTSP服务器:\n");
    printf("  端口: %d\n", rtsp_server.port);
    
    printf("流配置 (%zu 路流):\n", streams.size());
    for (size_t i = 0; i < streams.size(); ++i)
    {
        const auto& stream = streams[i];
        printf("  [%zu] 流ID: %s\n", i + 1, stream.id.c_str());
        printf("      名称: %s\n", stream.name.c_str());
        printf("      输入: %s\n", stream.input_url.c_str());
        printf("      输出: rtsp://localhost:%d/%s/%s\n", 
               rtsp_server.port, stream.output_app.c_str(), stream.output_stream.c_str());
        printf("      启用: %s\n", stream.enable ? "是" : "否");
        if (i < streams.size() - 1) printf("      ------\n");
    }
    
    // 向后兼容信息
    printf("向后兼容变量:\n");
    printf("  camera_rtsp: %s\n", camera_rtsp.c_str());
    printf("  model_root: %s\n", model_root.c_str());
    printf("  push_first: %s\n", push_first.c_str());
    printf("  push_second: %s\n", push_second.c_str());
    printf("  thread_num: %d\n", thread_num);
    printf("  port: %d\n", port);
}

std::vector<StreamConfig> Config::getEnabledStreams() const
{
    std::vector<StreamConfig> enabled;
    for (const auto& stream : streams)
    {
        if (stream.enable)
        {
            enabled.push_back(stream);
        }
    }
    return enabled;
}

StreamConfig Config::getStreamById(const std::string& id) const
{
    for (const auto& stream : streams)
    {
        if (stream.id == id)
        {
            return stream;
        }
    }
    return StreamConfig{}; // 返回空配置
}