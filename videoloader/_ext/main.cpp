#include <filesystem>

#include "videoloader.h"

using namespace huww::videoloader;

int main(int argc, char const *argv[])
{
    std::filesystem::path base = "/mnt/d/Downloads/answering_questions";
    VideoLoader loader;
    for (auto& f: std::filesystem::directory_iterator(base)) {
        loader.addVideoFile(f.path());
    }
    return 0;
}
