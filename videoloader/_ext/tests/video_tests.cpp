#include <gtest/gtest.h>

#include "video.h"

namespace vl = huww::videoloader;

TEST(VideoOpenFile, IsDir) {
    try {
        vl::video("/tmp");
        FAIL();
    } catch (vl::av_error &e) {
        EXPECT_EQ(e.code(), AVERROR(EISDIR));
    }
}

TEST(VideoOpenFile, NotExistFile) {
    try {
        vl::video("/some-non-exist-file");
        FAIL();
    } catch (std::system_error &e) {
        EXPECT_EQ(e.code().value(), ENOENT);
    }
}

class TestVideo : public ::testing::Test {
  protected:
    vl::video v;

    TestVideo() : v("./tests/test_video.mp4") {}
};

TEST_F(TestVideo, Open) {
    EXPECT_FALSE(v.is_sleeping());
}

TEST_F(TestVideo, GetBatch) {
    this->v.get_batch({1,2,3,4});
}

TEST_F(TestVideo, SleepAndGetBatch) {
    this->v.sleep();
    this->v.get_batch({1,2,3,4});
}
