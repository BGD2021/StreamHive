// 预处理

#include "preprocess.h"

#include "utils/logging.h"

#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"

void imgPreprocess(const cv::Mat &img, cv::Mat &img_resized, uint32_t width, uint32_t height)
{
    // img has to be 3 channels
    if (img.channels() != 3)
    {
        NN_LOG_ERROR("img has to be 3 channels");
        exit(-1);
    }

    cv::Mat img_rgb;
    cv::cvtColor(img, img_rgb, cv::COLOR_BGR2RGB);
    // resize img
    cv::resize(img_rgb, img_resized, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
}

// yolo的预处理
void cvimg2tensor(const cv::Mat &img, uint32_t width, uint32_t height, tensor_data_s &tensor)
{
    // img has to be 3 channels
    // if (img.channels() != 3)
    // {
    //     NN_LOG_ERROR("img has to be 3 channels");
    //     exit(-1);
    // }
    // // BGR to RGB
    // cv::Mat img_rgb;
    // //使用rk 的rga库进行颜色转换
    
    // cv::cvtColor(img, img_rgb, cv::COLOR_BGR2RGB);
    // // resize img
    // cv::Mat img_resized;
    // // NN_LOG_DEBUG("img size: %d, %d", img.cols, img.rows);
    // // NN_LOG_DEBUG("resize to: %d, %d", width, height);
    // cv::resize(img_rgb, img_resized, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
    // // BGR to RGB
    // memcpy(tensor.data, img_resized.data, tensor.attr.size);

    if (img.channels() != 3)
    {
        NN_LOG_ERROR("img has to be 3 channels");
        exit(-1);
    }

    // 创建 RGA 缓冲区句柄
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;

    // 将输入图像的内存地址导入为 RGA 缓冲区句柄
    src_handle = importbuffer_virtualaddr(img.data, img.total() * img.elemSize());
    if (!src_handle)
    {
        NN_LOG_ERROR("Failed to import source buffer");
        exit(-1);
    }

    // 创建目标缓冲区
    size_t dst_size = width * height * 3; // RGB 格式，每个像素 3 字节
    char* dst_buf = (char*)malloc(dst_size);
    if (!dst_buf)
    {
        NN_LOG_ERROR("Failed to allocate destination buffer");
        releasebuffer_handle(src_handle);
        exit(-1);
    }

    dst_handle = importbuffer_virtualaddr(dst_buf, dst_size);
    if (!dst_handle)
    {
        NN_LOG_ERROR("Failed to import destination buffer");
        free(dst_buf);
        releasebuffer_handle(src_handle);
        exit(-1);
    }

    // 设置源图像和目标图像的属性
    rga_buffer_t src_img = wrapbuffer_handle(src_handle, img.cols, img.rows, RK_FORMAT_RGB_888);
    rga_buffer_t dst_img = wrapbuffer_handle(dst_handle, width, height, RK_FORMAT_RGB_888);

    // 执行颜色转换和缩放
    int ret = imcvtcolor(src_img, dst_img, RK_FORMAT_BGR_888, RK_FORMAT_RGB_888);
    if (ret != IM_STATUS_SUCCESS)
    {
        NN_LOG_ERROR("Color conversion failed: %s", imStrError((IM_STATUS)ret));
        free(dst_buf);
        releasebuffer_handle(src_handle);
        releasebuffer_handle(dst_handle);
        exit(-1);
    }

    // 将缩放后的图像数据复制到 tensor
    memcpy(tensor.data, dst_buf, tensor.attr.size);

    // 释放资源
    free(dst_buf);
    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
}


void rga2tensor(const rga_buffer_t& rga_img, uint32_t width, uint32_t height, tensor_data_s& tensor)
{
    // rga_img: 原始图像（RGB888 格式）
    // width / height: 推理网络要求的输入大小
    // tensor.data: 用户分配好的推理输入内存空间

    // 分配目标缓冲区（你要 resize 到模型输入尺寸）
    size_t dst_size = width * height * 3; // RGB888
    void* dst_buf = malloc(dst_size);
    if (!dst_buf)
    {
        NN_LOG_ERROR("Failed to allocate destination buffer");
        exit(-1);
    }

    rga_buffer_handle_t dst_handle = importbuffer_virtualaddr(dst_buf, dst_size);
    if (!dst_handle)
    {
        NN_LOG_ERROR("Failed to import destination buffer");
        free(dst_buf);
        exit(-1);
    }

    rga_buffer_t dst_img = wrapbuffer_handle(dst_handle, width, height, RK_FORMAT_RGB_888);

    // RGA resize + 格式保持 RGB888
    IM_STATUS ret = imresize(rga_img, dst_img);
    if (ret != IM_STATUS_SUCCESS)
    {
        NN_LOG_ERROR("RGA resize failed: %s", imStrError(ret));
        releasebuffer_handle(dst_handle);
        free(dst_buf);
        exit(-1);
    }

    // 拷贝图像数据到 tensor
    memcpy(tensor.data, dst_buf, tensor.attr.size);

    // 清理资源
    releasebuffer_handle(dst_handle);
    free(dst_buf);
}
