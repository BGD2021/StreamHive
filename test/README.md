# ZMQ双服务器测试程序使用说明

## 概述

这个测试程序包含两个ZMQ服务器，用于模拟你的实际应用场景：

1. **RTSP地址服务器** (端口5555)：循环发送4个RTSP流地址
2. **报警信息服务器** (端口5556)：随机发送模拟的目标检测报警信息

## 文件说明

- `zmqServerTest.cpp` - 双服务器程序（发布者）
- `zmqClientTest.cpp` - 双客户端程序（订阅者）
- `Makefile` - 编译脚本
- `README.md` - 本说明文件

## 编译和运行

### 1. 安装依赖

```bash
# Ubuntu/Debian系统
make install-deps

# 或者手动安装
sudo apt-get install libzmq3-dev
```

### 2. 编译程序

```bash
# 编译所有程序
make all

# 或者分别编译
make zmqServerTest
make zmqClientTest
```

### 3. 运行测试

#### 方式一：使用Makefile

```bash
# 终端1：启动服务器
make run-server

# 终端2：启动客户端
make run-client

# 或者连接远程服务器
make run-client-remote SERVER_IP=192.168.1.100
```

#### 方式二：直接运行

```bash
# 终端1：启动服务器
./zmqServerTest

# 终端2：启动客户端 (连接本地)
./zmqClientTest

# 或者连接远程服务器
./zmqClientTest 192.168.1.100
```

## 服务器功能详细说明

### RTSP地址服务器 (端口5555)

- **发送频率**：每3秒发送一个地址
- **发送内容**：循环发送以下4个RTSP地址
  ```
  rtsp://192.168.10.107:3554/live/camera01
  rtsp://192.168.10.107:3554/live/camera02
  rtsp://192.168.10.107:3554/live/camera03
  rtsp://192.168.10.107:3554/live/camera04
  ```

### 报警信息服务器 (端口5556)

- **发送频率**：随机间隔1-4秒
- **发送内容**：随机生成的报警信息，格式如下
  ```
  流ID：后门摄像头 检测到目标 类别：person 置信度：0.698520;
  流ID：前门摄像头 检测到目标 类别：tie 置信度：0.292851;
  ```
- **随机元素**：
  - 流ID：后门摄像头、前门摄像头、侧门摄像头、大厅摄像头
  - 类别：person、car、tie、stop sign、bicycle、motorcycle、bus、truck
  - 置信度：0.2 - 0.95 之间的随机浮点数

## 客户端功能

客户端同时订阅两个服务器：
- 实时显示接收到的RTSP地址
- 实时显示接收到的报警信息
- 统计接收到的消息数量

## 示例输出

### 服务器端输出
```
=== ZMQ双服务器测试程序 ===
功能说明:
1. 端口5555: 循环发送4个RTSP流地址
2. 端口5556: 随机发送模拟报警信息
===============================
ZMQ测试服务器初始化成功
RTSP地址服务器: tcp://*:5555
报警信息服务器: tcp://*:5556
服务器已启动，按 Enter 键停止...
RTSP地址发送线程已启动...
报警信息发送线程已启动...
[RTSP] 发送: rtsp://192.168.10.107:3554/live/camera01
[ALARM] 发送: 流ID：后门摄像头 检测到目标 类别：person 置信度：0.698520;
[ALARM] 发送: 流ID：前门摄像头 检测到目标 类别：tie 置信度：0.292851;
```

### 客户端输出
```
=== ZMQ双客户端测试程序 ===
使用默认服务器: localhost
===============================
ZMQ测试客户端初始化成功
连接到RTSP服务器: tcp://localhost:5555
连接到报警服务器: tcp://localhost:5556
客户端已启动，按 Enter 键停止...
RTSP地址接收线程已启动...
报警信息接收线程已启动...
[RTSP #1] 接收到: rtsp://192.168.10.107:3554/live/camera01
[ALARM #1] 接收到: 流ID：后门摄像头 检测到目标 类别：person 置信度：0.698520;
[ALARM #2] 接收到: 流ID：前门摄像头 检测到目标 类别：tie 置信度：0.292851;
```

## 网络调试助手测试

如果你想用网络调试助手测试，需要注意：

1. **ZMQ不是原生TCP**：ZMQ有自己的协议，直接用TCP调试助手可能无法正确解析
2. **建议方法**：
   - 使用这个客户端程序进行测试
   - 或者使用支持ZMQ的工具
   - 或者修改服务器代码支持原生TCP

## 故障排除

### 编译错误
```bash
# 检查是否安装了ZMQ库
pkg-config --libs libzmq
pkg-config --cflags libzmq

# 如果没有安装
sudo apt-get install libzmq3-dev
```

### 连接错误
- 检查防火墙设置
- 确认端口5555和5556没有被占用
- 检查服务器IP地址是否正确

### 运行时错误
- 确保服务器已启动
- 检查网络连接
- 查看错误信息进行诊断

## 自定义修改

如果需要修改测试数据，可以编辑 `zmqServerTest.cpp` 中的以下数组：

```cpp
// RTSP地址
std::vector<std::string> rtsp_urls = {
    "你的RTSP地址1",
    "你的RTSP地址2",
    // ...
};

// 流ID
std::vector<std::string> stream_ids = {
    "你的流ID1",
    "你的流ID2",
    // ...
};

// 检测类别
std::vector<std::string> class_names = {
    "你的类别1",
    "你的类别2",
    // ...
};
```
