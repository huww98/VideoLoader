#pragma once

#include "video.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <optional>
#include <thread>
#include <vector>

namespace huww {
namespace videoloader {

namespace dataset_load_schedule_detail {
struct crop_schedule {
    int x, y, w, h;
};
struct scale_schedule {
    int w, h;
};
struct video {
    videoloader::video &video;
    std::vector<size_t> frame_indices;
    std::optional<crop_schedule> crop;
    std::optional<scale_schedule> scale;

    auto get_batch(dlpack_pool *pool = nullptr) { return video.get_batch(frame_indices, pool); }
};
using batch = std::vector<video>;
using schedule = std::vector<batch>;
}; // namespace dataset_load_schedule_detail
using dataset_load_schedule = dataset_load_schedule_detail::schedule;

class video_batch_dlpack {};
class batch_output_buffer;
struct load_task;

/**
 * Estimate how fast an event happens.
 * 
 * Should only be updated by one thread.
 */
class speed_estimator {
  public:
    using duration_t = std::chrono::duration<double, std::milli>;
    using clock_t = std::chrono::steady_clock;

  private:
    clock_t::duration average_duration;

    struct Event {
        int weight;
        clock_t::time_point time;
    };
    clock_t::time_point start_time;
    std::deque<Event> events;
    int total_weight;
    std::atomic<double> _speed;

  public:
    speed_estimator(clock_t::duration average_duration);
    void start();
    void finish(int item_count = 1);
    void finish(clock_t::duration duration, int item_count = 1);
    /**
     * Get the estimated speed (duration per event)
     * 
     * Thread safe.
     */
    duration_t speed();
};

class video_dataset_loader {
    std::vector<batch_output_buffer> output_buffer;
    std::vector<load_task> load_tasks;
    std::atomic<size_t> next_task_index = 0;
    std::atomic<bool> running = false;

    std::atomic<int> active_worker_count = 0;
    std::mutex active_worker_m;
    struct worker;
    std::vector<worker> workers;

    using clock_t = std::chrono::steady_clock;
    clock_t::time_point start_time;
    clock_t::duration warmup_duration = std::chrono::seconds(1);
    size_t max_preload = 512;
    std::atomic<size_t> consumed = 0; /**< Number of videos comsumed by `get_next_batch()` */

    size_t next_batch_index = 0;
    size_t last_batch_size = 0;
    speed_estimator consume_speed;

    /** Main entrypoint of worker threads. */
    void load_worker_main(int worker_index);

    /**
     * Determine the number of active workers
     *
     * Adjust number of active workers accroding to the estimated comsume and load speed, to avoid
     * consume too much CPU.
     *
     * \note This is thread safe and will be called from every worker and get_batch thread.
     */
    void schedule_workers();
    int calc_needed_workers();

  public:
    class no_more_batch final : public std::logic_error {
      public:
        no_more_batch() : std::logic_error("No more batch to load.") {}
    };

    video_dataset_loader(const dataset_load_schedule &schedule);
    ~video_dataset_loader();
    void start(int max_threads);

    /**
     * Join all worker threads
     *
     * After stopped, `start` can be called again.
     * After stopped, the threads that call `get_next_batch()` to get not fully loaded batch will
     * still be blocked.
     */
    void stop();

    /**
     * Get next batch of data
     *
     * Will block until at least one batch of data avaliable. Can only used in one thread.
     */
    std::vector<video_dlpack::ptr> get_next_batch();

    /** Not implemented yet */
    video_batch_dlpack get_next_scaled_batch();
};

} // namespace videoloader
} // namespace huww
