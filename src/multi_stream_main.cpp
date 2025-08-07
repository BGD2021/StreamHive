#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include "config/config.hpp"
#include "stream/streamManager.hpp"
#include "utils/msgServer.hpp"

void printHelp() {
    printf("============================================\n");
    printf("          StreamHive 多路视频流处理服务\n");
    printf("============================================\n");
    printf("可用命令:\n");
    printf("  start <stream_id>  - 启动指定流\n");
    printf("  stop <stream_id>   - 停止指定流\n");
    printf("  restart <stream_id>- 重启指定流\n");
    printf("  status             - 查看流状态\n");
    printf("  list               - 列出所有流配置\n");
    printf("  health             - 健康检查\n");
    printf("  startall           - 启动所有启用的流\n");
    printf("  stopall            - 停止所有流\n");
    printf("  restartall         - 重启所有流\n");
    printf("  help               - 显示帮助信息\n");
    printf("  quit               - 退出程序\n");
    printf("============================================\n");
}

void printStreamList(const MultiStreamManager& manager) {
    auto streams = manager.getAllStreams();
    printf("\n=== 流配置列表 ===\n");
    if (streams.empty()) {
        printf("没有配置的流\n");
        return;
    }
    
    for (size_t i = 0; i < streams.size(); ++i) {
        const auto& stream = streams[i];
        printf("[%zu] ID: %s\n", i + 1, stream.id.c_str());
        printf("    名称: %s\n", stream.name.c_str());
        printf("    输入: %s\n", stream.input_url.c_str());
        printf("    输出: /%s/%s\n", stream.output_app.c_str(), stream.output_stream.c_str());
        printf("    启用: %s\n", stream.enable ? "是" : "否");
        if (i < streams.size() - 1) printf("    ------\n");
    }
}

int main(int argc, char *argv[]) {
    const char *config_file = nullptr;
    std::thread msg_thread;
    // 参数解析
    for (int i = 1; i < argc; i += 2) {
        if (argv[i][0] != '-') {
            printf("参数错误: %s\n", argv[i]);
            return -1;
        }
        switch (argv[i][1]) {
        case 'h':
            printf("用法: %s -f <config.json>\n", argv[0]);
            printf("-h 显示帮助信息\n");
            printf("-f 配置文件路径\n");
            return 0;
        case 'f':
            if (i + 1 < argc) {
                config_file = argv[i + 1];
            } else {
                printf("缺少配置文件路径\n");
                return -1;
            }
            break;
        default:
            printf("未知参数: %s\n", argv[i]);
            return -1;
        }
    }

    if (!config_file) {
        printf("错误: 必须指定配置文件\n");
        printf("用法: %s -f config.json\n", argv[0]);
        return -1;
    }

    // 加载配置
    Config config(config_file);
    if (!config.mState) {
        printf("错误: 无法加载配置文件 %s\n", config_file);
        return -1;
    }

    // 显示配置信息
    config.show();
    // if (config.rtsp_server.port > 0) {
    //     msg_thread = std::thread([&](){
    //         msgServer server(5555);//rtsp
    //         server.sendRtspAddressThread(config,5);
    //     });
    //     msg_thread.detach();
    // }

    // 创建多流管理器
    MultiStreamManager stream_manager(config);

    // 显示帮助信息
    printHelp();

    stream_manager.start();

    // 命令行交互循环
    std::string input;
    while (true) {
        std::cout << "\nStreamHive[" << stream_manager.getRunningStreamCount() 
                  << "/" << stream_manager.getStreamCount() << "]> ";
        
        if (!std::getline(std::cin, input)) {
            break; // EOF
        }

        if (input.empty()) {
            continue;
        }

        // 解析命令
        std::istringstream iss(input);
        std::string command;
        iss >> command;

        if (command == "quit" || command == "q" || command == "exit") {
            printf("正在退出程序...\n");
            msg_thread.detach(); // 确保消息线程安全退出
            stream_manager.stop();
            break;
        }
        else if (command == "help" || command == "h") {
            printHelp();
        }
        else if (command == "status" || command == "st") {
            stream_manager.showStatus();
        }
        else if (command == "list" || command == "ls") {
            printStreamList(stream_manager);
        }
        else if (command == "health" || command == "hc") {
            stream_manager.healthCheck();
        }
        else if (command == "startall") {
            printf("启动所有启用的流\n");
            stream_manager.start();
        }
        else if (command == "stopall") {
            printf("停止所有流\n");
            stream_manager.stop();
        }
        else if (command == "restartall") {
            printf("重启所有流\n");
            stream_manager.restartAll();
        }
        else if (command == "start") {
            std::string stream_id;
            if (iss >> stream_id) {
                stream_manager.startStream(stream_id);
            } else {
                printf("用法: start <stream_id>\n");
            }
        }
        else if (command == "stop") {
            std::string stream_id;
            if (iss >> stream_id) {
                stream_manager.stopStream(stream_id);
            } else {
                printf("用法: stop <stream_id>\n");
            }
        }
        else if (command == "restart") {
            std::string stream_id;
            if (iss >> stream_id) {
                stream_manager.restartStream(stream_id);
            } else {
                printf("用法: restart <stream_id>\n");
            }
        }
        else {
            printf("未知命令: %s (输入 help 查看帮助)\n", command.c_str());
        }
    }

    printf("StreamHive 多路流管理器已安全退出\n");
    return 0;
}
