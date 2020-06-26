#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <system_error>

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include "tar_iterator.h"
#include "video.h"

namespace huww {
namespace videoloader {

template <typename Filter> std::vector<video> open_video_tar(std::string tar_path, Filter filter) {
    std::vector<video> videos;
    for (auto &entry : tar_iterator(tar_path, tar_options::advise_sequential)) {
        if (entry.type() != huww::tar_entry_type::file) {
            continue;
        }
        if (!filter(entry)) {
            continue;
        }
        entry.will_need_content();
        auto v = video({
            .path = tar_path,
            .start_pos = entry.content_start_position(),
            .file_size = entry.file_size(),
            .external_stream = &entry.begin_read_content(),
        });
        v.sleep();
        videos.push_back(std::move(v));
    }
    return videos;
}

std::vector<video> open_video_tar(std::string tar_path);

template <typename Filter>
std::vector<video> open_video_tar(std::string tar_path, Filter filter, int max_threads) {
    SPDLOG_TRACE("Open tar file with max {} threads", max_threads);
    std::condition_variable task_finished;
    std::mutex task_finished_m;
    std::atomic<bool> all_finished = false;
    struct worker {
        std::condition_variable got_task;
        std::mutex got_task_m;
        std::atomic<bool> busy = false;
        file_io::file_spec task;
        std::optional<video> *output;
        std::thread thread;
    };
    std::vector<worker> workers(max_threads);
    for (auto &w : workers) {
        w.thread = std::thread([&] {
            std::ifstream local_tar_stream(tar_path, std::ios::binary);
            while (true) {
                {
                    std::unique_lock lk(w.got_task_m);
                    while (true) {
                        if (w.busy.load(std::memory_order_acquire)) {
                            SPDLOG_TRACE("Worker {} got task.", static_cast<void *>(&w));
                            break;
                        }
                        if (all_finished.load(std::memory_order_relaxed)) {
                            SPDLOG_TRACE("Worker {} exit.", static_cast<void *>(&w));
                            return;
                        }
                        SPDLOG_TRACE("Worker {} waiting for task.", static_cast<void *>(&w));
                        w.got_task.wait(lk);
                    }
                }
                local_tar_stream.seekg(w.task.start_pos);
                w.task.external_stream = &local_tar_stream;
                *(w.output) = video(w.task);
                w.output->value().sleep();
                w.busy.store(false, std::memory_order_release);
                { std::lock_guard lk(task_finished_m); }
                task_finished.notify_one();
            }
        });
    }

    std::deque<std::optional<video>> videos;
    for (auto &entry : tar_iterator(tar_path, tar_options::advise_sequential)) {
        if (entry.type() != huww::tar_entry_type::file) {
            continue;
        }
        if (!filter(entry)) {
            continue;
        }
        SPDLOG_TRACE("Processing entry {}", entry.path());
        entry.will_need_content();
        worker *idle_worker = nullptr;
        auto search_idle_worker = [&idle_worker, &workers]() {
            for (auto &w : workers) {
                if (!w.busy) {
                    idle_worker = &w;
                    return true;
                }
            }
            return false;
        };
        {
            std::unique_lock lk(task_finished_m);
            while (true) {
                if (search_idle_worker()) {
                    break;
                }
                task_finished.wait(lk);
            }
        }
        SPDLOG_TRACE("Distribute task to worker {}.", static_cast<void *>(idle_worker));
        {
            std::lock_guard(idle_worker->got_task_m);
            idle_worker->task = {
                .path = tar_path,
                .start_pos = entry.content_start_position(),
                .file_size = entry.file_size(),
            };
            videos.push_back({});
            idle_worker->output = &*videos.rbegin();
            idle_worker->busy.store(true, std::memory_order_release);
        }
        idle_worker->got_task.notify_one();
    }
    all_finished.store(true);
    SPDLOG_TRACE("All tar entry processed. Wait for worker threads");
    for (auto &w : workers) {
        {
            std::unique_lock lk(w.got_task_m);
        }
        w.got_task.notify_one();
    }
    for (auto &w : workers) {
        w.thread.join();
    }

    std::vector<video> output_videos;
    output_videos.reserve(videos.size());
    for (auto &v : videos) {
        output_videos.push_back(std::move(v.value()));
    }
    return output_videos;
}

std::vector<video> open_video_tar(std::string tar_path, int max_threads);

} // namespace videoloader
} // namespace huww
