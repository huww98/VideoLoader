#include <chrono>
#include <filesystem>
#include <iostream>

#include "video.h"
#include "video_dataset_loader.h"
#include "video_tar.h"

#include <spdlog/spdlog.h>
#include <unistd.h>

using namespace huww::videoloader;
using namespace std;

constexpr int num_threads = 2;
constexpr int batch_size = 32;

vector<size_t> frames(const video &v) {
    size_t num = min(v.num_frames(), (size_t)16);
    vector<size_t> frames;
    for (size_t i = 0; i < num; i++) {
        frames.push_back(i);
    }
    return frames;
}

vector<video> open_videos() {
    // std::filesystem::path base = "/mnt/d/Downloads/answering_questions";
    std::filesystem::path base = "/tmp/answering_questions";
    vector<video> videos;
    for (auto &f : std::filesystem::recursive_directory_iterator(base)) {
        if (f.path().extension() != ".mp4") {
            continue;
        }
        // cout << f.path() << endl;
        auto v = video(f.path());
        v.sleep();
        // video.get_batch({14, 15});
        videos.push_back(move(v));
        if (videos.size() % 1000 == 0) {
            spdlog::info("{} videos opened...", videos.size());
        }
    }
    return videos;
}

vector<video> open_tar_videos() {
    return open_video_tar("/media/huww/44A02C63A02C5E24/Downloads/answering_questions.tar",
                          [n = 0](const huww::tar_entry &entry) mutable {
                              n++;
                              if (n % 1000 == 0) {
                                  spdlog::info("{} videos opened...", n);
                              }
                              return true;
                          }, 4);
}

int main(int argc, char const *argv[]) {
    spdlog::set_pattern("[thread %t] [%T.%f] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::trace);

    spdlog::info("Opening all videos");
    // auto videos = open_videos();
    auto videos = open_tar_videos();

    spdlog::info("{} videos opened.", videos.size());
    // return 0;

    dataset_load_schedule sche;
    sche.push_back({});
    for (auto &v : videos) {
        if (sche.rbegin()->size() == batch_size) {
            sche.push_back({});
        }
        sche.rbegin()->push_back({
            .video = v,
            .frame_indices = frames(v),
        });
    }

    video_dataset_loader dsloader(sche);
    spdlog::info("Start loading using {} threads", num_threads);
    auto t1 = chrono::high_resolution_clock::now();
    dsloader.start(num_threads);
    while (true) {
        try {
            auto data = dsloader.get_next_batch();
            spdlog::info("Got a batch of {} clips", data.size());
        } catch (video_dataset_loader::no_more_batch &) {
            break;
        }
    }
    dsloader.stop();
    auto t2 = chrono::high_resolution_clock::now();
    spdlog::info("Finished. Time: {} ms",
                 chrono::duration_cast<chrono::milliseconds>(t2 - t1).count());

    return 0;
}
