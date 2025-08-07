# 设置最低版本号
cmake_minimum_required(VERSION 3.11 FATAL_ERROR)
# 设置项目名称
project(StreamHive VERSION 0.0.1 LANGUAGES CXX)

# 输出系统信息
message(STATUS "System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")

# 设置编译器
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置库架构
set(LIB_ARCH "aarch64")
set(DEVICE_NAME "RK3576")

link_directories(
    mpp_api
    mk_api
)

#  rknn_api 文件夹路径
set(RKNN_API_PATH ${CMAKE_CURRENT_SOURCE_DIR}/librknn_api)
#  rknn_api include 路径
set(RKNN_API_INCLUDE_PATH ${RKNN_API_PATH}/include)
#  rknn_api lib 路径
#set(RKNN_API_LIB_PATH ${RKNN_API_PATH}/${LIB_ARCH}/librknnrt.so)
set(RKNN_API_LIB_PATH /usr/lib/librknnrt.so)


# 寻找OpenCV库，使用自定义的OpenCV_DIR
set(3RDPARTY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
set(RGA_DIR ${3RDPARTY_PATH}/rga/${DEVICE_NAME})
#set(RGA_LIB ${RGA_DIR}/lib/Linux/${LIB_ARCH}/librga.so)
set(RGA_LIB ${RGA_DIR}/lib/librga.so)
# set(OpenCV_DIR ${3RDPARTY_PATH}/opencv/opencv-linux-${LIB_ARCH}/share/OpenCV)
find_package(OpenCV REQUIRED)
# 输出OpenCV信息
message(STATUS "include path: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "${OpenCV_VERSION}")
#寻找jsoncpp库
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
#寻找zmq库
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

include_directories(${ZMQ_INCLUDE_DIRS})
link_directories(${ZMQ_LIBRARY_DIRS})

# 用来搜索头文件的目录
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${RKNN_API_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${RGA_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/mpp_api/include
    ${CMAKE_CURRENT_LIST_DIR}/mk_api/include
    ${JSONCPP_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
)

# 构建预处理和后处理库
add_library(nn_process SHARED
            src/process/preprocess.cpp
            src/process/postprocess.cpp
)
# 链接库
target_link_libraries(nn_process
    ${OpenCV_LIBS}
    ${RGA_LIB}
)

# 构建自定义封装API库
add_library(rknn_engine SHARED src/engine/rknn_engine.cpp) 
# 链接库
target_link_libraries(rknn_engine
    ${RKNN_API_LIB_PATH}
)



# yolov5_lib
add_library(yolov5_lib SHARED src/model/yolov5.cpp)
# 链接库
target_link_libraries(yolov5_lib
    rknn_engine
    nn_process
)


# 线程池库
add_library(threadpool_lib SHARED
        src/threadPool/framePool.cpp
)

target_link_libraries(threadpool_lib
        yolov5_lib
        draw_lib
        ${OpenCV_LIBS}
        ${RGA_LIB}
)

# draw_lib
add_library(draw_lib SHARED src/draw/cv_draw.cpp)
# 链接库
target_link_libraries(draw_lib
    ${OpenCV_LIBS}
)

#配置文件
add_library(config SHARED src/config/config.cpp)
target_link_libraries(config
    ${JSONCPP_LIBRARIES}
)

#视频处理
add_library(stream SHARED
    src/stream/avPullStream.cpp
    src/stream/avPushStream.cpp
    src/RtspWorker/worker.cpp
    src/stream/streamManager.cpp
    src/stream/matPool.cpp
)
target_link_libraries(stream
    rockchip_mpp 
    utils 
    mk_api 
    ${OpenCV_LIBS}
    ${RGA_LIB}
    threadpool_lib
)


# # 测试usb_to_rtsp
# add_executable(usb_to_rtsp 
#         src/usb_to_rtsp.cpp
#         src/rkmedia/utils/mpp_decoder.cpp
#         src/rkmedia/utils/mpp_encoder.cpp
# )

# # 链接库
# target_link_libraries(usb_to_rtsp
#         rockchip_mpp 
#         utils 
#         mk_api 
#         ${OpenCV_LIBS}
#         ${RGA_LIB}
#         draw_lib
#         yolov5_lib
#         threadpool_lib
#         pthread
# )




# 测试yolov8_video_track
add_executable(rtsp_to_rtsp 
        src/rtsp_to_rtsp.cpp
        src/rkmedia/utils/mpp_decoder.cpp
        src/rkmedia/utils/mpp_encoder.cpp
)

# 链接库
target_link_libraries(rtsp_to_rtsp
        rockchip_mpp 
        utils 
        mk_api 
        ${OpenCV_LIBS}
        ${RGA_LIB}
)


# # 主函数
# add_executable(main 
#         src/main.cpp
#         src/rkmedia/utils/mpp_decoder.cpp
#         src/rkmedia/utils/mpp_encoder.cpp
# )

# # 链接库
# target_link_libraries(main
#         rockchip_mpp 
#         utils 
#         mk_api 
#         ${OpenCV_LIBS}
#         ${RGA_LIB}
#         draw_lib
#         yolov5_lib
#         threadpool_lib
#         pthread
#         config
# )

add_executable(StreamHive
        src/multi_stream_main.cpp
        src/rkmedia/utils/mpp_decoder.cpp
        src/rkmedia/utils/mpp_encoder.cpp
        src/utils/msgServer.cpp
)

target_link_libraries(StreamHive
        rockchip_mpp 
        utils 
        mk_api 
        ${OpenCV_LIBS}
        ${RGA_LIB}
        draw_lib
        yolov5_lib
        threadpool_lib
        pthread
        config
        stream
        ${ZMQ_LIBRARIES}
)


