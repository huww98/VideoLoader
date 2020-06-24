#include "video_tar.h"

namespace huww {
namespace videoloader {

std::vector<video> open_video_tar(std::string tar_path) {
    return open_video_tar(tar_path, [](const tar_entry &_) { return true; });
}

std::vector<video> open_video_tar(std::string tar_path, int max_threads) {
    return open_video_tar(tar_path, [](const tar_entry &_) { return true; }, max_threads);
}

} // namespace videoloader
} // namespace huww
