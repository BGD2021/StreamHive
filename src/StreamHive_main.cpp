#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <opencv2/opencv.hpp>

#include "config/config.hpp"

#include "stream/avPushStream.hpp"
#include "stream/avPullStream.hpp"
#include "RtspWorker/worker.hpp"

int main(int argc, char *argv[])
{
    const char *file = NULL;

    for (int i = 1; i < argc; i += 2)
    {
        if (argv[i][0] != '-')
        {
            printf("parameter error:%s\n", argv[i]);
            return -1;
        }
        switch (argv[i][1])
        {
        case 'h':
        {
            // 打印help信息
            printf("-h 打印参数配置信息并退出\n");
            printf("-f 配置文件   如：-f config.json \n");
            system("pause\n");
            exit(0);
            return -1;
        }
        case 'f':
        {
            file = argv[i + 1];
            break;
        }
        default:
        {
            printf("set parameter error:%s\n", argv[i]);
            return -1;
        }
        }
    }

    if (file == NULL)
    {
        printf("failed to read config file\n");
        return -1;
    }

    Config file_config(file);
    if (!file_config.mState)
    {
        printf("failed to read config file: %s\n", file);
        return -1;
    }
    // 打印配置文件内容
    file_config.show();
    // mk_rtsp_server_start(3554, 0);

    RtspWorker *worker = new RtspWorker(file_config.camera_rtsp,
                                        file_config.port,
                                        file_config.push_first,
                                        file_config.push_second,
                                        file_config.model_root,
                                        file_config.thread_num);
    worker->start();
    for(;;){
        char input = getchar();
        
        if (input == 'q' || input == 'Q') {
            printf("退出程序...\n");
            worker->stop();
            delete worker;
            worker = nullptr;
            break;
        }
    }
}