#include <gtest/gtest.h>

#include "video_tar.h"

namespace vl = huww::videoloader;

TEST(OpenVideosInTar, Open) {
    auto videos = vl::open_video_tar("./tests/tar/test_videos.tar");
    EXPECT_EQ(3, videos.size());
}

TEST(OpenVideosInTar, OpenFiltered) {
    auto videos =
        vl::open_video_tar("./tests/tar/test_videos.tar", [](const huww::tar_entry &entry) {
            return entry.path() == "answering_questions/-g3JhkJRVY4_000333_000343.mp4";
        });
    EXPECT_EQ(1, videos.size());
}
