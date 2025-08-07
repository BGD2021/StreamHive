// 预处理

#ifndef RK3588_DEMO_PREPROCESS_H
#define RK3588_DEMO_PREPROCESS_H

#include <opencv2/opencv.hpp>
#include "types/datatype.h"

void imgPreprocess(const cv::Mat &img, cv::Mat &img_resized, uint32_t width, uint32_t height);
void cvimg2tensor(const cv::Mat &img, uint32_t width, uint32_t height, tensor_data_s &tensor);
// void rga2tensor(const rga_buffer_t& rga_img, uint32_t width, uint32_t height, tensor_data_s& tensor);
#endif // RK3588_DEMO_PREPROCESS_H
