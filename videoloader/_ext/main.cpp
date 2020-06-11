#include <chrono>
#include <filesystem>
#include <iostream>

#include "VideoDatasetLoader.h"
#include "videoloader.h"
#include <unistd.h>
#include <spdlog/spdlog.h>

using namespace huww::videoloader;
using namespace std;

constexpr int numThreads = 2;
constexpr int batchSize = 32;

int main(int argc, char const *argv[]) {
    spdlog::set_pattern("[thread %t] %+");
    spdlog::set_level(spdlog::level::trace);

    // std::filesystem::path base = "/mnt/d/Downloads/answering_questions";
    std::filesystem::path base = "/tmp/answering_questions";
    VideoLoader loader;
    vector<Video> videos;
    for (auto &f : std::filesystem::directory_iterator(base)) {
        // cout << f.path() << endl;
        auto video = loader.addVideoFile(f.path());
        video.sleep();
        // video.getBatch({14, 15});
        videos.push_back(move(video));
    }

    spdlog::info("{} videos opened.", videos.size());

    vector<size_t> frames;
    for (size_t i = 0; i < 16; i++) {
        frames.push_back(i);
    }

    DatasetLoadSchedule sche;
    sche.push_back({});
    for (auto &v : videos) {
        if (sche.rbegin()->size() == batchSize) {
            sche.push_back({});
        }
        sche.rbegin()->push_back({
            .video = v,
            .frameIndices = frames,
        });
    }

    VideoDatasetLoader dsloader(sche);
    spdlog::info("Start loading using {} threads", numThreads);
    auto t1 = chrono::high_resolution_clock::now();
    dsloader.start(numThreads);
    while (true) {
        try {
            auto data = dsloader.getNextBatch();
            spdlog::info("Got a batch of {} clips", data.size());
        } catch (VideoDatasetLoader::NoMoreBatch &) {
            break;
        }
    }
    dsloader.stop();
    auto t2 = chrono::high_resolution_clock::now();
    spdlog::info("Finished. Time: {}ms", chrono::duration_cast<chrono::milliseconds>(t2 - t1).count());

    return 0;
}
