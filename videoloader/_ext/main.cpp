#include <chrono>
#include <filesystem>
#include <iostream>

#include "VideoDatasetLoader.h"
#include "videoloader.h"
#include <spdlog/spdlog.h>
#include <unistd.h>

using namespace huww::videoloader;
using namespace std;

constexpr int numThreads = 2;
constexpr int batchSize = 32;

vector<size_t> frames(const Video& v) {
    size_t num = min(v.numFrames(), (size_t)16);
    vector<size_t> frames;
    for (size_t i = 0; i < num; i++) {
        frames.push_back(i);
    }
    return frames;
}

int main(int argc, char const *argv[]) {
    spdlog::set_pattern("[thread %t] %+");
    spdlog::set_level(spdlog::level::trace);

    // std::filesystem::path base = "/mnt/d/Downloads/answering_questions";
    std::filesystem::path base = "/tmp/answering_questions";
    VideoLoader loader;
    vector<Video> videos;
    for (auto &f : std::filesystem::recursive_directory_iterator(base)) {
        if (f.path().extension() != ".mp4") {
            continue;
        }
        // cout << f.path() << endl;
        auto video = loader.addVideoFile(f.path());
        video.sleep();
        // video.getBatch({14, 15});
        videos.push_back(move(video));
        if (videos.size() % 1000 == 0) {
            spdlog::info("{} videos opened...", videos.size());
        }
    }

    spdlog::info("{} videos opened.", videos.size());

    DatasetLoadSchedule sche;
    sche.push_back({});
    for (auto &v : videos) {
        if (sche.rbegin()->size() == batchSize) {
            sche.push_back({});
        }
        sche.rbegin()->push_back({
            .video = v,
            .frameIndices = frames(v),
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
    spdlog::info("Finished. Time: {} ms",
                 chrono::duration_cast<chrono::milliseconds>(t2 - t1).count());

    return 0;
}
