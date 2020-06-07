#include <filesystem>
#include <iostream>
#include <chrono>

#include "VideoDatasetLoader.h"
#include "videoloader.h"
#include <unistd.h>

using namespace huww::videoloader;
using namespace std;

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

    DatasetLoadSchedule sche;
    sche.push_back({});
    for (auto &v : videos) {
        if (sche.rbegin()->size() == 16) {
            sche.push_back({});
        }
        sche.rbegin()->push_back({
            .video = v,
            .frameIndices = {0, 1, 2, 3},
        });
    }

    VideoDatasetLoader dsloader(sche);
    constexpr int numThreads = 2;
    cout << "Start loading using " << numThreads << " threads" << endl;
    auto t1 = chrono::high_resolution_clock::now();
    dsloader.start(numThreads);
    while (dsloader.hasNextBatch())
    {
        auto data = dsloader.getNextBatch();
        cout << "Got a batch of " << data.size() << " clips" << endl;
    }
    dsloader.stop();
    auto t2 = chrono::high_resolution_clock::now();
    cout << "Finished. Time: " << chrono::duration_cast<chrono::milliseconds>(t2 - t1).count() << "ms" << endl;

    return 0;
}
