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

    auto getBatch(DLPackPool *pool = nullptr) { return video.getBatch(frameIndices, pool); }
};
using Batch = std::vector<Video>;
using schedule = std::vector<Batch>;
}; // namespace DatasetLoadScheduleDetail
using DatasetLoadSchedule = DatasetLoadScheduleDetail::schedule;

class VideoBatchDLPack {};
class BatchOutputBuffer;
struct LoadTask;

/**
 * Estimate how fast an event happens.
 * 
 * Should only be updated by one thread.
 */
class SpeedEstimator {
  public:
    using duration_t = std::chrono::duration<double, std::milli>;
    using clock_t = std::chrono::steady_clock;

  private:
    clock_t::duration averageDuration;

    struct Event {
        int weight;
        clock_t::time_point time;
    };
    clock_t::time_point startTime;
    std::deque<Event> events;
    int totalWeight;
    std::atomic<double> _speed;

  public:
    SpeedEstimator(clock_t::duration averageDuration);
    void start();
    void finish(int itemCount = 1);
    void finish(clock_t::duration duration, int itemCount = 1);
    /**
     * Get the estimated speed (duration per event)
     * 
     * Thread safe.
     */
    duration_t speed();
};

class VideoDatasetLoader {
    std::vector<BatchOutputBuffer> outputBuffer;
    std::vector<LoadTask> loadTasks;
    std::atomic<size_t> nextTaskIndex = 0;
    std::atomic<bool> running = false;

    std::atomic<int> activeWorkerCount = 0;
    std::mutex activeWorker_m;
    struct Worker;
    std::vector<Worker> workers;

    using clock_t = std::chrono::steady_clock;
    clock_t::time_point startTime;
    clock_t::duration warmupDuration = std::chrono::seconds(1);
    size_t maxPreload = 512;
    std::atomic<size_t> consumed = 0; /**< Number of videos comsumed by `getNextBatch()` */

    size_t nextBatchIndex = 0;
    size_t lastBatchSize = 0;
    SpeedEstimator consumeSpeed;

    /** Main entrypoint of worker threads. */
    void loadWorker(int workerIndex);

    /**
     * Determine the number of active workers
     *
     * Adjust number of active workers accroding to the estimated comsume and load speed, to avoid
     * consume too much CPU.
     *
     * \note This is thread safe and will be called from every worker and getBatch thread.
     */
    void scheduleWorkers();
    int calcNeededWorkers();

  public:
    class NoMoreBatch final : public std::logic_error {
      public:
        NoMoreBatch() : std::logic_error("No more batch to load.") {}
    };

    VideoDatasetLoader(const DatasetLoadSchedule &schedule);
    ~VideoDatasetLoader();
    void start(int maxThreads);

    /**
     * Join all worker threads
     *
     * After stopped, `start` can be called again.
     * After stopped, the threads that call `getNextBatch()` to get not fully loaded batch will
     * still be blocked.
     */
    void stop();

    /**
     * Get next batch of data
     *
     * Will block until at least one batch of data avaliable. Can only used in one thread.
     */
    std::vector<VideoDLPack::ptr> getNextBatch();

    /** Not implemented yet */
    VideoBatchDLPack getNextScaledBatch();
};

} // namespace videoloader
} // namespace huww
