#include "VideoDatasetLoader.h"

#include <atomic>

#include <assert.h>
#include <pthread.h>

namespace huww {

#if defined __linux__
constexpr bool onLinux = true;
#else
constexpr bool onLinux = false;
#endif

namespace videoloader {

class BatchOutputBuffer {
    std::vector<std::optional<VideoDLPack>> buffer;
    std::atomic<size_t> numFilled = 0;
    std::condition_variable fullCV;
    std::mutex fullCV_m;

  public:
    explicit BatchOutputBuffer(int num_videos) : buffer(num_videos) {}
    bool full() { return numFilled.load(std::memory_order_acquire) == buffer.size(); }
    void waitUntilFull() {
        if (full()) {
            return;
        }
        std::unique_lock lk(fullCV_m);
        fullCV.wait(lk, [this] { return this->full(); });
    }
    void add(int index, VideoDLPack &&data) {
        assert(!buffer[index].has_value());
        buffer[index] = std::move(data);
        auto previousFilled = numFilled.fetch_add(1, std::memory_order_release);
        if (previousFilled + 1 == buffer.size()) {
            {
                std::lock_guard lk(fullCV_m);
            }
            fullCV.notify_all();
        }
    }
    std::vector<VideoDLPack> transferData() {
        std::vector<VideoDLPack> data;
        data.reserve(this->buffer.size());
        for (auto &&b : this->buffer) {
            data.push_back(std::move(b.value()));
        }
        this->buffer.clear();
        return data;
    }
};

static std::vector<BatchOutputBuffer> initOutputBuffer(const DatasetLoadSchedule &schedule) {
    std::vector<size_t> batchSizes;
    batchSizes.reserve(schedule.size());
    for (auto &s : schedule) {
        batchSizes.push_back(s.size());
    }
    return std::vector<BatchOutputBuffer>(batchSizes.begin(), batchSizes.end());
}

struct LoadTask {
    DatasetLoadScheduleDetail::Video video;
    size_t batchIndex;
    size_t videoIndex;
};

static std::vector<LoadTask> initLoadTask(const DatasetLoadSchedule &schedule) {
    int numTasks = 0;
    for (auto &s : schedule) {
        numTasks += s.size();
    }
    std::vector<LoadTask> tasks;
    tasks.reserve(numTasks);
    for (size_t i = 0; i < schedule.size(); i++) {
        auto &s = schedule[i];
        for (size_t j = 0; j < s.size(); j++) {
            tasks.push_back({
                .video = s[j],
                .batchIndex = i,
                .videoIndex = j,
            });
        }
    }
    return tasks;
}

VideoDatasetLoader::VideoDatasetLoader(const DatasetLoadSchedule &schedule)
    : outputBuffer(initOutputBuffer(schedule)), loadTasks(initLoadTask(schedule)) {}

VideoDatasetLoader::~VideoDatasetLoader() {
    if (this->running) {
        this->stop();
    }
};

void VideoDatasetLoader::start(int maxThreads) {
    if (this->running.exchange(true, std::memory_order_relaxed)) {
        throw std::logic_error("This loader is already running");
    }
    for (int i = 0; i < maxThreads; i++) {
        auto w = std::thread([this] { this->loadWorker(); });
        if constexpr (onLinux) {
            std::stringstream ss;
            ss << "VideoDatasetLoader #" << i;
            pthread_setname_np(w.native_handle(), ss.str().c_str());
        }
        this->workers.push_back(std::move(w));
    }
}

void VideoDatasetLoader::stop() {
    if (!this->running.exchange(false, std::memory_order_relaxed)) {
        throw std::logic_error("This loader is already stopped");
    }
    for (auto &w : this->workers) {
        w.join();
    }
    this->workers.clear();
}

void VideoDatasetLoader::loadWorker() {
    while (this->running.load(std::memory_order_relaxed)) {
        auto taskIndex = this->nextTaskIndex.fetch_add(1, std::memory_order_relaxed);
        if (taskIndex >= this->loadTasks.size()) {
            break;
        }
        auto &task = this->loadTasks[taskIndex];
        auto &output = this->outputBuffer[task.batchIndex];
        output.add(task.videoIndex, task.video.getBatch());
    }
}

bool VideoDatasetLoader::hasNextBatch() {
    return this->nextBatchIndex.load() + 1 < this->outputBuffer.size();
}

std::vector<VideoDLPack> VideoDatasetLoader::getNextBatch() {
    auto batchIndex = this->nextBatchIndex.fetch_add(1);
    auto &output = this->outputBuffer[batchIndex];
    output.waitUntilFull();
    return output.transferData();
}

VideoBatchDLPack VideoDatasetLoader::getNextScaledBatch() {
    throw std::logic_error("Not implemented yet");
}

} // namespace videoloader
} // namespace huww
