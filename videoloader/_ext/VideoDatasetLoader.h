#pragma once

#include "videoloader.h"
#include <atomic>
#include <condition_variable>
#include <optional>
#include <vector>
#include <thread>


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

class VideoDatasetLoader {
    std::vector<BatchOutputBuffer> outputBuffer;
    std::vector<LoadTask> loadTasks;
    std::atomic<size_t> nextTaskIndex = 0;
    std::atomic<bool> running = false;
    std::vector<std::thread> workers;

    std::atomic<size_t> nextBatchIndex = 0;

    void loadWorker();

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
