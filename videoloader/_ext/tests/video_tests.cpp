#include <gtest/gtest.h>

#include "video.h"

namespace vl = huww::videoloader;

TEST(VideoOpenFile, IsDir) {
    try {
        vl::video("/tmp");
        FAIL();
    } catch (vl::av_error& e) {
        EXPECT_EQ(e.code(), AVERROR(EISDIR));
    }
}

TEST(VideoOpenFile, NotExistFile) {
    try {
        vl::video("/some-non-exist-file");
        FAIL();
    } catch (std::system_error& e) {
        EXPECT_EQ(e.code().value(), ENOENT);
    }
}

TEST(VideoOpenFile, Normal) {
    vl::video("./tests/test_video.mp4");
}
