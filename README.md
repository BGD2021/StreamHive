# 🚀 StreamHive

基于瑞芯微 RK3576 平台的多路视频流智能检测系统，支持同时接入最多 4 路网络摄像头，结合 YOLOv5 实现边缘 AI 实时推理，转推叠加识别结果的视频流，广泛适用于智慧厨房、工业安全等场景。

系统全流程基于硬件加速组件（RGA、RKMPP、RKNN），充分发挥 RK3576 在音视频处理与 AI 推理方面的性能优势。引入线程池机制实现多路流并发推理，通过 JSON 配置摄像头，灵活可拓展，支持脱网部署。

---

## 📚 Table of Contents

- [✨ Features](#-features)
- [📁 Project Structure](#-project-structure)
- [🧰 Dependencies](#-dependencies)
- [⚡ Quick Start](#-quick-start)
- [🖥️ Frontend Preview](#-frontend-preview)
- [📄 License](#-license)

---

## ✨ Features

✅ **多路视频流处理**：支持并发处理多路 RTSP 摄像头流  
✅ **边缘计算部署**：在板端完成推理，无需上传云端，保障隐私与数据安全  
✅ **高实时性**：基于 ZLMediaKit 实现视频拉流与推送，延迟最低可达 900ms  
✅ **多端可视化**：推理后的视频通过 RTSP/FLV 推流，可灵活接入多种流媒体前端  
✅ **配置简洁灵活**：通过 JSON 文件配置摄像头与推理参数，支持热插拔  
✅ **模块化设计**：结构清晰，便于扩展识别功能或替换算法  
✅ **性能极致优化**：使用 RKNN 推理引擎 + RGA 图像处理 + RKMPP 编解码，大幅减轻 CPU 压力，充分利用多核多线程架构

---

## 📁 Project Structure

```bash
.
├── build/                     # 编译输出目录
├── config.json                # 摄像头与系统配置文件
├── CMakeLists.txt             # CMake 构建脚本
├── README.md                  # 项目说明文档
├── src/                       # 源码目录
│   ├── multi_stream_main.cpp   # 主程序入口
│   ├── stream/                # 流处理模块
│   │   ├── avPullStream.cpp   # 视频拉流解码
│   │   ├── avPushStream.cpp   # 编码与推流
│   │   ├── matPool.cpp        # Mat 内存池
│   │   ├── streamManager.cpp  # 多路流调度器
│   ├── RtspWorker/            # RTSP 工作线程封装
│   ├── utils/                 # 工具模块
│   │   ├── config.cpp         # 配置解析
│   │   ├── msgServer.cpp      # ZeroMQ 消息服务
│   │   ├── safeQueue.cpp      # 线程安全队列
├── test/                      # 单元测试模块
│   ├── zmqServerTest.cpp
│   ├── zmqClientTest.cpp
│   ├── Makefile
│   └── README.md
````

---

## 🧰 Dependencies

请确保系统中已安装以下依赖：

| 组件         | 版本要求                                                           |
| ---------- | -------------------------------------------------------------- |
| CMake      | ≥ 3.10                                                         |
| OpenCV     | ≥ 4.5.0                                                        |
| ZeroMQ     | ≥ 4.3.4                                                        |
| JsonCpp    | ≥ 1.9.4                                                        |
| RKNPU2 SDK | 对应 RK3576 平台                                                   |
| 编解码库       | `librknn_api.so`, `librga.so`, `librkmedia.so`, `librk_mpi.so` |

---

## ⚡ Quick Start

### 1️⃣ 开发板准备

* 确保 RK3576 已刷入包含 RKNN、RGA、RKMPP 的 Linux 镜像；
* 使用支持 RTSP 的网络摄像头并连接开发板；
* 准备好 YOLOv5 模型并转换为 `.rknn` 格式，放入 `weights/` 目录；
* 配置 `config.json` 文件，添加摄像头流信息和推理参数。

---

### 2️⃣ 编译项目

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)  # 或 make -j4
```

---

### 3️⃣ 运行系统

```bash
sudo ./StreamHive -f <path_to_config.json>
```

示例：

```bash
sudo ./StreamHive -f ../config.json
```

---

### 4️⃣ 查看日志

* 程序启动后将输出摄像头连接状态、推理信息、推流状态等；
* 如需排查错误，请关注异常提示和堆栈输出。

---

## 🖥️ Frontend Preview

* 项目前端基于 Qt 开发，通过 ZeroMQ 连接后端推理结果；
* 支持：

  * 实时视频显示；
  * 报警信息弹窗；
  * 多摄像头画面切换；
  * 日志查看与配置更新。

👉 *Qt 项目路径（待补充）*

---

## 📄 License

本项目遵循 MIT 开源协议，详细内容请见 [`LICENSE`](./LICENSE)。

---

## 📬 Contact

如有问题，欢迎通过 Issues 提交反馈，或联系：

📧 [1239503460@qq.com](mailto:1239503460@qq.com)

