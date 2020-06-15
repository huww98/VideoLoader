#include "video_dataset_loader.h"

#include <atomic>
#include <chrono>

#include <assert.h>
#include <pthread.h>
#include <spdlog/spdlog.h>

namespace huww {

#if defined __linux__
constexpr bool on_linux = true;
#else
constexpr bool on_linux = false;
#endif

namespace videoloader {

using namespace std::chrono_literals;

speed_estimator::speed_estimator(clock_t::duration average_duration)
    : average_duration(average_duration), total_weight(0), _speed(NAN) {
    this->events.push_back({
        .weight = 0,
        .time = {},
    });
    this->start();
}

void speed_estimator::start() { this->start_time = clock_t::now(); }

void speed_estimator::finish(int item_count) {
    auto duration = clock_t::now() - start_time;
    this->finish(duration, item_count);
}

void speed_estimator::finish(clock_t::duration duration, int item_count) {
    assert(item_count >= 0);
    if (item_count == 0) {
        return;
    }

    auto next_time_point = duration + events.rbegin()->time;
    events.push_back({
        .weight = item_count,
        .time = next_time_point,
    });
    total_weight += item_count;
    while (next_time_point - events.begin()->time > average_duration && events.size() > 2) {
        auto &expired = *events.begin();
        total_weight -= expired.weight;
        events.pop_front();
    }

    if (events.size() > 1) {
        duration_t dur = events.rbegin()->time - events.begin()->time;
        auto speed = (dur / total_weight).count();
        this->_speed.store(speed, std::memory_order_relaxed);
    }
}

auto speed_estimator::speed() -> duration_t {
    return duration_t(this->_speed.load(std::memory_order_relaxed));
}

class batch_output_buffer {
    std::vector<video_dlpack::ptr> buffer;
    std::atomic<size_t> num_filled = 0;
    std::condition_variable full_cv;
    std::mutex full_cv_m;

  public:
    explicit batch_output_buffer(int num_videos) : buffer(num_videos) {}
    bool full() { return num_filled.load(std::memory_order_acquire) == buffer.size(); }
    void wait_until_full() {
        if (full()) {
            return;
        }
        std::unique_lock lk(full_cv_m);
        full_cv.wait(lk, [this] { return this->full(); });
    }
    void add(int index, video_dlpack::ptr &&data) {
        assert(!buffer[index]);
        buffer[index] = std::move(data);
        auto previous_filled = num_filled.fetch_add(1, std::memory_order_release);
        if (previous_filled + 1 == buffer.size()) {
            {
                std::lock_guard lk(full_cv_m);
            }
            full_cv.notify_all();
        }
    }
    std::vector<video_dlpack::ptr> transfer_data() { return std::move(this->buffer); }
    auto size() const noexcept { return this->buffer.size(); }
};

static std::vector<batch_output_buffer> init_output_buffer(const dataset_load_schedule &schedule) {
    std::vector<size_t> batch_sizes;
    batch_sizes.reserve(schedule.size());
    for (auto &s : schedule) {
        batch_sizes.push_back(s.size());
    }
    return std::vector<batch_output_buffer>(batch_sizes.begin(), batch_sizes.end());
}

struct load_task {
    dataset_load_schedule_detail::video video;
    size_t batch_index;
    size_t video_index;
};

static std::vector<load_task> init_load_task(const dataset_load_schedule &schedule) {
    int num_tasks = 0;
    for (auto &s : schedule) {
        num_tasks += s.size();
    }
    std::vector<load_task> tasks;
    tasks.reserve(num_tasks);
    for (size_t i = 0; i < schedule.size(); i++) {
        auto &s = schedule[i];
        for (size_t j = 0; j < s.size(); j++) {
            tasks.push_back({
                .video = s[j],
                .batch_index = i,
                .video_index = j,
            });
        }
    }
    return tasks;
}

video_dataset_loader::video_dataset_loader(const dataset_load_schedule &schedule)
    : output_buffer(init_output_buffer(schedule)), load_tasks(init_load_task(schedule)),
      consume_speed(10s) {}

video_dataset_loader::~video_dataset_loader() {
    if (this->running) {
        this->stop();
    }
};

struct video_dataset_loader::worker {
    std::thread thread;
    std::condition_variable active_cv;
    speed_estimator speed;

    worker() : speed(3s) {}
};

void video_dataset_loader::start(int max_threads) {
    if (this->running.exchange(true, std::memory_order_relaxed)) {
        throw std::logic_error("This loader is already running");
    }
    this->start_time = clock_t::now();
    this->active_worker_count = max_threads;
    this->workers = std::vector<worker>(max_threads);
    for (int i = 0; i < max_threads; i++) {
        auto &w = this->workers[i];
        w.thread = std::thread([this, i] { this->load_worker_main(i); });
        if constexpr (on_linux) {
            std::stringstream ss;
            ss << "video_dataset_loader #" << i;
            pthread_setname_np(w.thread.native_handle(), ss.str().c_str());
        }
    }
}

void video_dataset_loader::stop() {
    if (!this->running.exchange(false, std::memory_order_relaxed)) {
        throw std::logic_error("This loader is already stopped");
    }
    // Wake up all workers.
    this->active_worker_count = this->workers.size();
    { std::lock_guard lk(this->active_worker_m); }
    for (auto &w : this->workers) {
        w.active_cv.notify_one();
    }

    for (auto &w : this->workers) {
        w.thread.join();
    }
    this->workers.clear();
}

int video_dataset_loader::calc_needed_workers() {
    int active_worker_count = this->active_worker_count.load(std::memory_order_relaxed);

    auto consumed = this->consumed.load(std::memory_order_relaxed);
    auto loaded = this->next_task_index.load(std::memory_order_relaxed);
    auto can_load = this->max_preload - (loaded - consumed);
    if (can_load <= 0) {
        // Hit max preload limit, pause all workers.
        SPDLOG_DEBUG("Hit max preload limit");
        return 0;
    }
    auto running_time = clock_t::now() - start_time;
    if (running_time < warmup_duration) {
        // Warming up, use all workers.
        SPDLOG_DEBUG("Warming up");
        return workers.size();
    }
    auto consume_speed = this->consume_speed.speed();
    if (std::isnan(consume_speed.count())) {
        SPDLOG_DEBUG("No enough consume speed estimation");
        return workers.size();
    }

    // Estimate average load speed.
    speed_estimator::duration_t load_speed;
    if (active_worker_count == 0) {
        // Recover from pause.
        load_speed = this->workers[0].speed.speed();
    } else {
        load_speed = {};
        for (int i = 0; i < active_worker_count; i++) {
            load_speed += this->workers[i].speed.speed();
        }
        if (std::isnan(load_speed.count())) {
            SPDLOG_DEBUG("No enough load speed estimation");
            return workers.size();
        }
        load_speed /= active_worker_count;
    }

    // We want load speed slightly faster than comsume.
    int new_active_worker_count = static_cast<int>(std::ceil(load_speed / (consume_speed * 0.95)));
    // Don't overshoot preload limit too much.
    new_active_worker_count = std::min(new_active_worker_count, (int)can_load);
    new_active_worker_count = std::min(new_active_worker_count, (int)workers.size());
    SPDLOG_DEBUG("Scheduling workers. consume speed: {:.3f} ms; load speed: {:.3f} ms; workers: {}",
                 consume_speed.count(), load_speed.count(), new_active_worker_count);
    return new_active_worker_count;
}

void video_dataset_loader::schedule_workers() {
    int new_active_worker_count = this->calc_needed_workers();
    this->active_worker_count.store(new_active_worker_count, std::memory_order_relaxed);
    { std::lock_guard lk(this->active_worker_m); }
    for (int i = 0; i < new_active_worker_count; i++) {
        workers[i].active_cv.notify_one();
    }
}

void video_dataset_loader::load_worker_main(int worker_index) {
    auto &worker = this->workers[worker_index];
    dlpack_pool pool;
    while (this->running.load(std::memory_order_relaxed)) {
        auto task_index = this->next_task_index.fetch_add(1, std::memory_order_relaxed);
        if (task_index >= this->load_tasks.size()) {
            break;
        }

        worker.speed.start();
        auto &task = this->load_tasks[task_index];
        auto &output = this->output_buffer[task.batch_index];
        output.add(task.video_index, task.video.get_batch(&pool));
        task.video.video.sleep();
        worker.speed.finish(1);

        this->schedule_workers();
        auto is_active = [this, worker_index] {
            return this->active_worker_count.load(std::memory_order_relaxed) > worker_index;
        };
        if (!is_active()) {
            std::unique_lock lk(this->active_worker_m);
            worker.active_cv.wait(lk, is_active);
        }
    }
}

std::vector<video_dlpack::ptr> video_dataset_loader::get_next_batch() {
    auto batch_index = this->next_batch_index++;
    if (batch_index >= this->output_buffer.size()) {
        throw no_more_batch();
    }
    this->consume_speed.finish(last_batch_size);

    auto &output = this->output_buffer[batch_index];
    output.wait_until_full();
    this->consumed.fetch_add(output.size(), std::memory_order_relaxed);
    this->schedule_workers(); // should goes after `consumed` updated

    this->last_batch_size = output.size();
    auto loaded_batch = output.transfer_data();
    this->consume_speed.start();
    return loaded_batch;
}

video_batch_dlpack video_dataset_loader::get_next_scaled_batch() {
    throw std::logic_error("Not implemented yet");
}

} // namespace videoloader
} // namespace huww
