#include "VideoDatasetLoader.h"

#include <atomic>
#include <chrono>

#include <assert.h>
#include <pthread.h>
#include <spdlog/spdlog.h>

namespace huww {

#if defined __linux__
constexpr bool onLinux = true;
#else
constexpr bool onLinux = false;
#endif

namespace videoloader {

using namespace std::chrono_literals;

SpeedEstimator::SpeedEstimator(clock_t::duration averageDuration)
    : averageDuration(averageDuration), totalWeight(0), _speed(NAN) {
    this->events.push_back({
        .weight = 0,
        .time = {},
    });
    this->start();
}

void SpeedEstimator::start() { this->startTime = clock_t::now(); }

void SpeedEstimator::finish(int itemCount) {
    auto duration = clock_t::now() - startTime;
    this->finish(duration, itemCount);
}

void SpeedEstimator::finish(clock_t::duration duration, int itemCount) {
    assert(itemCount >= 0);
    if (itemCount == 0) {
        return;
    }

    auto nextTimePoint = duration + events.rbegin()->time;
    events.push_back({
        .weight = itemCount,
        .time = nextTimePoint,
    });
    totalWeight += itemCount;
    while (nextTimePoint - events.begin()->time > averageDuration && events.size() > 2) {
        auto &expired = *events.begin();
        totalWeight -= expired.weight;
        events.pop_front();
    }

    if (events.size() > 1) {
        duration_t dur = events.rbegin()->time - events.begin()->time;
        auto speed = (dur / totalWeight).count();
        this->_speed.store(speed, std::memory_order_relaxed);
    }
}

auto SpeedEstimator::speed() -> duration_t {
    return duration_t(this->_speed.load(std::memory_order_relaxed));
}

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
    auto size() const noexcept { return this->buffer.size(); }
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
    : outputBuffer(initOutputBuffer(schedule)), loadTasks(initLoadTask(schedule)),
      consumeSpeed(10s) {}

VideoDatasetLoader::~VideoDatasetLoader() {
    if (this->running) {
        this->stop();
    }
};

struct VideoDatasetLoader::Worker {
    std::thread thread;
    std::condition_variable activeCV;
    SpeedEstimator speed;

    Worker() : speed(3s) {}
};

void VideoDatasetLoader::start(int maxThreads) {
    if (this->running.exchange(true, std::memory_order_relaxed)) {
        throw std::logic_error("This loader is already running");
    }
    this->startTime = clock_t::now();
    this->activeWorkerCount = maxThreads;
    this->workers = std::vector<Worker>(maxThreads);
    for (int i = 0; i < maxThreads; i++) {
        auto &w = this->workers[i];
        w.thread = std::thread([this, i] { this->loadWorker(i); });
        if constexpr (onLinux) {
            std::stringstream ss;
            ss << "VideoDatasetLoader #" << i;
            pthread_setname_np(w.thread.native_handle(), ss.str().c_str());
        }
    }
}

void VideoDatasetLoader::stop() {
    if (!this->running.exchange(false, std::memory_order_relaxed)) {
        throw std::logic_error("This loader is already stopped");
    }
    // Wake up all workers.
    this->activeWorkerCount = this->workers.size();
    { std::lock_guard lk(this->activeWorker_m); }
    for (auto &w : this->workers) {
        w.activeCV.notify_one();
    }

    for (auto &w : this->workers) {
        w.thread.join();
    }
    this->workers.clear();
}

int VideoDatasetLoader::calcNeededWorkers() {
    int activeWorkerCount = this->activeWorkerCount.load(std::memory_order_relaxed);

    auto consumed = this->consumed.load(std::memory_order_relaxed);
    auto loaded = this->nextTaskIndex.load(std::memory_order_relaxed);
    auto canLoad = this->maxPreload - (loaded - consumed);
    if (canLoad <= 0) {
        // Hit max preload limit, pause all workers.
        SPDLOG_DEBUG("Hit max preload limit");
        return 0;
    }
    auto runningTime = clock_t::now() - startTime;
    if (runningTime < warmupDuration) {
        // Warming up, use all workers.
        SPDLOG_DEBUG("Warming up");
        return workers.size();
    }
    auto consumeSpeed = this->consumeSpeed.speed();
    if (std::isnan(consumeSpeed.count())) {
        SPDLOG_DEBUG("No enough consume speed estimation");
        return workers.size();
    }

    // Estimate average load speed.
    SpeedEstimator::duration_t loadSpeed;
    if (activeWorkerCount == 0) {
        // Recover from pause.
        loadSpeed = this->workers[0].speed.speed();
    } else {
        loadSpeed = {};
        for (int i = 0; i < activeWorkerCount; i++) {
            loadSpeed += this->workers[i].speed.speed();
        }
        if (std::isnan(loadSpeed.count())) {
            SPDLOG_DEBUG("No enough load speed estimation");
            return workers.size();
        }
        loadSpeed /= activeWorkerCount;
    }

    // We want load speed slightly faster than comsume.
    int newActiveWorkerCount = static_cast<int>(std::ceil(loadSpeed / (consumeSpeed * 0.95)));
    // Don't overshoot preload limit too much.
    newActiveWorkerCount = std::min(newActiveWorkerCount, (int)canLoad);
    newActiveWorkerCount = std::min(newActiveWorkerCount, (int)workers.size());
    SPDLOG_DEBUG("Scheduling workers. consume speed: {:.3f} ms; load speed: {:.3f} ms; workers: {}",
                 consumeSpeed.count(), loadSpeed.count(), newActiveWorkerCount);
    return newActiveWorkerCount;
}

void VideoDatasetLoader::scheduleWorkers() {
    int newActiveWorkerCount = this->calcNeededWorkers();
    this->activeWorkerCount.store(newActiveWorkerCount, std::memory_order_relaxed);
    { std::lock_guard lk(this->activeWorker_m); }
    for (int i = 0; i < newActiveWorkerCount; i++) {
        workers[i].activeCV.notify_one();
    }
}

void VideoDatasetLoader::loadWorker(int workerIndex) {
    auto &worker = this->workers[workerIndex];
    while (this->running.load(std::memory_order_relaxed)) {
        auto taskIndex = this->nextTaskIndex.fetch_add(1, std::memory_order_relaxed);
        if (taskIndex >= this->loadTasks.size()) {
            break;
        }

        worker.speed.start();
        auto &task = this->loadTasks[taskIndex];
        auto &output = this->outputBuffer[task.batchIndex];
        output.add(task.videoIndex, task.video.getBatch());
        task.video.video.sleep();
        worker.speed.finish(1);

        this->scheduleWorkers();
        auto isActive = [this, workerIndex] {
            return this->activeWorkerCount.load(std::memory_order_relaxed) > workerIndex;
        };
        if (!isActive()) {
            std::unique_lock lk(this->activeWorker_m);
            worker.activeCV.wait(lk, isActive);
        }
    }
}

std::vector<VideoDLPack> VideoDatasetLoader::getNextBatch() {
    auto batchIndex = this->nextBatchIndex++;
    if (batchIndex >= this->outputBuffer.size()) {
        throw NoMoreBatch();
    }
    this->consumeSpeed.finish(lastBatchSize);

    auto &output = this->outputBuffer[batchIndex];
    output.waitUntilFull();
    this->consumed.fetch_add(output.size(), std::memory_order_relaxed);
    this->scheduleWorkers(); // should goes after `consumed` updated

    this->lastBatchSize = output.size();
    auto loadedBatch = output.transferData();
    this->consumeSpeed.start();
    return loadedBatch;
}

VideoBatchDLPack VideoDatasetLoader::getNextScaledBatch() {
    throw std::logic_error("Not implemented yet");
}

} // namespace videoloader
} // namespace huww
