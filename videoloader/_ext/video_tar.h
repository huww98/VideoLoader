#pragma once

#include <string>
#include <vector>

#include "video.h"
#include "tar_iterator.h"

namespace huww {
namespace videoloader {

template<typename Filter>
std::vector<video> open_video_tar(std::string tar_path, Filter filter) {
    std::vector<video> videos;
    for (auto &entry : tar_iterator(tar_path)) {
        if (entry.type() != huww::tar_entry_type::file) {
            continue;
        }
        if (!filter(entry)) {
            continue;
        }
        auto v = video({
            .path = tar_path,
            .start_pos = entry.content_start_position(),
            .file_size = entry.file_size(),
            .external_stream = &entry.begin_read_content(),
        });
        v.sleep();
        videos.push_back(std::move(v));
    }
    return videos;
}

std::vector<video> open_video_tar(std::string tar_path);

} // namespace videoloader
} // namespace huww
