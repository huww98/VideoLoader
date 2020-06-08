#pragma once

#include "videoloader.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <optional>
#include <thread>
#include <vector>

namespace huww {
namespace videoloader {

namespace DatasetLoadScheduleDetail {
struct Crop {
    int x, y, w, h;
};
struct Scale {
    int w, h;
};
struct Video {
    videoloader::Video &video;
    std::vector<size_t> frameIndices;
    std::optional<Crop> crop;
    std::optional<Scale> scale;

    auto getBatch() { return video.getBatch(frameIndices); }
};
using Batch = std::vector<Video>;
using schedule = std::vector<Batch>;
}; // namespace DatasetLoadScheduleDetail
using DatasetLoadSchedule = DatasetLoadScheduleDetail::schedule;

class VideoBatchDLPack {};
class BatchOutputBuffer;
struct LoadTask;

class SpeedEstimator {
    using duration_t = std::chrono::duration<double>;
    using clock_t = std::chrono::steady_clock;
    clock_t::duration averageDuration;

    struct Event {
        int weight;
        clock_t::time_point time;
    };
    std::deque<Event> events;
    int totalWeight = 0;
    std::atomic<double> _speed;

  public:
    SpeedEstimator(clock_t::duration averageDuration) : averageDuration(averageDuration) {}
    void finish(int itemCount = 1);
    duration_t speed();
};

class VideoDatasetLoader {
    std::vector<BatchOutputBuffer> outputBuffer;
    std::vector<LoadTask> loadTasks;
    std::atomic<size_t> nextTaskIndex = 0;
    std::atomic<bool> running = false;

    struct Worker;
    std::vector<Worker> workers;

    std::atomic<size_t> nextBatchIndex = 0;
    SpeedEstimator getBatchSpeed;

    void loadWorker(int workerIndex);

  public:
    VideoDatasetLoader(const DatasetLoadSchedule &schedule);
    ~VideoDatasetLoader();
    void start(int maxThreads);
    void stop();
    bool hasNextBatch();
    std::vector<VideoDLPack> getNextBatch();
    VideoBatchDLPack getNextScaledBatch();
};

} // namespace videoloader
} // namespace huww
