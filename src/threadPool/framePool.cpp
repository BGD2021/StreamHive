#include "framePool.hpp"
#include <atomic>
#include "draw/cv_draw.h"


framePool::framePool(const std::string model_path, const int thread_num) {
  this->thread_num_ = thread_num;
  this->model_path_ = model_path;
  this->Init();
}


void framePool::Init() {
    try{
        //配置n个线程
        this->pool_ = std::make_shared<ThreadPool>(this->thread_num_);
        //每个线程加载一个模型
        for (int i = 0; i < this->thread_num_; i++) {
            auto model = std::make_shared<Yolov5>();
            model->LoadModel(this->model_path_.c_str());
            //将模型添加到线程池中
            this->models_.push_back(model);
        }
    }catch (const std::exception &e) {
        std::cerr << "Error initializing framePool: " << e.what() << std::endl;
    }
}

void framePool::DeInit() {
    this->pool_.reset();
    this->models_.clear();
}

void framePool::inferenceThread(std::shared_ptr<cv::Mat> src){
    pool_->enqueue([this, src]() {
        try {
            // 检查输入图像的有效性
            if (!src || src->empty() || src->data == nullptr) {
                std::cerr << "Invalid input image in inference thread" << std::endl;
                return;
            }
            
            auto model_id = this->get_model_id(); // 获取模型ID

            auto model = this->models_[model_id];
            auto objects = std::make_shared<std::vector<Detection>>();
            model->Run(*src, *objects); // 注意Run参数类型
            DrawDetections(*src, *objects);
            std::lock_guard<std::mutex> lock(this->image_results_mutex_);
            this->image_results_.push({src, objects});
        } catch (const std::exception &e) {
            std::cerr << "Error in inference thread: " << e.what() << std::endl;
        }
    });
}
int framePool::get_model_id() {
  std::lock_guard<std::mutex> lock(id_mutex_);
  int mode_id = id;
  id++;
  if (id == thread_num_) {
    id = 0;
  }
  return mode_id;
}


detection_t framePool::GetImageResultFromQueue() {
    std::lock_guard<std::mutex> lock(this->image_results_mutex_);
    if (this->image_results_.empty()) {
        return detection_t{}; // 返回空的rga_buffer_t对象
    }
    auto result = this->image_results_.front();
    this->image_results_.pop();
    // // 将结果转换成rga格式
    // rga_buffer_t rga_result = wrapbuffer_virtualaddr((void *)result.src->data, result.src->cols, result.src->rows, RK_FORMAT_RGB_888);
    return result;
}

framePool::~framePool() = default;

int framePool::GetTasksSize() { return pool_->TasksSize(); }

// 添加获取结果队列大小的方法
int framePool::GetResultQueueSize() {
    std::lock_guard<std::mutex> lock(this->image_results_mutex_);
    return this->image_results_.size();
}