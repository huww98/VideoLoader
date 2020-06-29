#include <gtest/gtest.h>

#include "video_tar.h"

namespace vl = huww::videoloader;

TEST(OpenVideosInTar, Open) {
    auto videos = vl::open_video_tar("./tests/tar/test_videos.tar");
    EXPECT_EQ(3, videos.size());
}

TEST(OpenVideosInTar, OpenMT) {
    auto videos = vl::open_video_tar("./tests/tar/test_videos.tar", 4);
    EXPECT_EQ(3, videos.size());
}

TEST(OpenVideosInTar, OpenMT1) {
    auto videos = vl::open_video_tar("./tests/tar/test_videos.tar", 1);
    EXPECT_EQ(3, videos.size());
}

TEST(OpenVideosInTar, OpenMT0Throws) {
    EXPECT_THROW(vl::open_video_tar("", 0), std::logic_error);
}

TEST(OpenVideosInTar, OpenFiltered) {
    auto videos =
        vl::open_video_tar("./tests/tar/test_videos.tar", [](const huww::tar_entry &entry) {
            return entry.path() == "answering_questions/-g3JhkJRVY4_000333_000343.mp4";
        });
    EXPECT_EQ(1, videos.size());
}

TEST(OpenVideosInTar, OpenFilterThrows) {
    try {
        vl::open_video_tar("./tests/tar/test_videos.tar", [](const huww::tar_entry &entry) {
            if (entry.path() == "answering_questions/-g3JhkJRVY4_000333_000343.mp4") {
                throw std::runtime_error("TestError");
            }
            return true;
        });
        FAIL();
    } catch (std::runtime_error &e) {
        EXPECT_STREQ(e.what(), "TestError");
    }
}
