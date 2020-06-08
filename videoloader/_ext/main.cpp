#include <chrono>
#include <filesystem>
#include <iostream>

#include "VideoDatasetLoader.h"
#include "videoloader.h"
#include <unistd.h>

using namespace huww::videoloader;
using namespace std;

constexpr int numThreads = 1;
constexpr int batchSize = 32;

int main(int argc, char const *argv[]) {
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

    cout << videos.size() << " Videos opened" << endl;

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
    cout << "Start loading using " << numThreads << " threads" << endl;
    auto t1 = chrono::high_resolution_clock::now();
    dsloader.start(numThreads);
    while (dsloader.hasNextBatch()) {
        auto data = dsloader.getNextBatch();
        cout << "Got a batch of " << data.size() << " clips" << endl;
    }
    dsloader.stop();
    auto t2 = chrono::high_resolution_clock::now();
    cout << "Finished. Time: " << chrono::duration_cast<chrono::milliseconds>(t2 - t1).count()
         << "ms" << endl;

    return 0;
}
