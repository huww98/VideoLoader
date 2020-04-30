#include <filesystem>
#include <iostream>

#include "videoloader.h"
#include <unistd.h>

using namespace huww::videoloader;
using namespace std;

int main(int argc, char const *argv[])
{
    // std::filesystem::path base = "/mnt/d/Downloads/answering_questions";
    std::filesystem::path base = "/tmp/answering_questions";
    VideoLoader loader;
    vector<Video> videos;
    try {
        for (auto& f: std::filesystem::directory_iterator(base)) {
            // cout << f.path() << endl;
            auto video = loader.addVideoFile(f.path());
            video.sleep();
            video.getBatch({14, 15});
            videos.push_back(move(video));
            break;
        }
    } catch (std::runtime_error &e) {
        cerr << "[Excpetion]: " << e.what() << endl;
    }

    return 0;
}
