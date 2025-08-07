#include <queue>
#include "threadPool.hpp"
#include "model/yolov5.h"
#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"

typedef struct {
   std::shared_ptr<cv::Mat> src;
   std::shared_ptr<std::vector<Detection>> objects;
} detection_t;

class framePool {
 public:
    framePool(const std::string model_path, const int thread_num);
    ~framePool();
    void Init();
    void DeInit();
    void inferenceThread(std::shared_ptr<cv::Mat> src);
    detection_t GetImageResultFromQueue();
    int GetTasksSize();
    int GetResultQueueSize(); // 新增：获取结果队列大小
    int get_model_id();

 private:
    int thread_num_{1};
    std::string model_path_{"null"};
    std::string label_path_{"null"};
    uint32_t id{0};
    std::shared_ptr<ThreadPool> pool_;
    std::queue<detection_t> image_results_; // 调整队列类型
    std::vector<std::shared_ptr<Yolov5>> models_;
    std::mutex id_mutex_;
    std::mutex image_results_mutex_;
};