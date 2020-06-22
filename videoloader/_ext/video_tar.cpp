#include "video_tar.h"

namespace huww {
namespace videoloader {

std::vector<video> open_video_tar(std::string tar_path) {
    return open_video_tar(tar_path, [](const tar_entry &_) { return true; });
}

} // namespace videoloader
} // namespace huww
