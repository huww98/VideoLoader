#include <filesystem>
#include <iostream>

#include "videoloader.h"

using namespace huww::videoloader;
using namespace std;

int main(int argc, char const *argv[])
{
    // std::filesystem::path base = "/mnt/d/Downloads/answering_questions";
    std::filesystem::path base = "/tmp/answering_questions";
    VideoLoader loader;
    try {
        for (auto& f: std::filesystem::directory_iterator(base)) {
            // cout << f.path() << endl;
            auto video = loader.addVideoFile(f.path());
            video.sleep();
            video.getBatch({14, 15});
            // break;
        }
    } catch (std::runtime_error &e) {
        cerr << "[Excpetion]: " << e.what() << endl;
    }

    return 0;
}
