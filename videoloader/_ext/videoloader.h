#pragma once

#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}
#include "FileIO.h"

namespace huww {
namespace videoloader {

struct PacketIndexEntry {
    int64_t pts;
    int keyFrameIndex;
    int packetIndex;
};

class Video {
  private:
    AVIOContextPtr ioContext;
    AVFormatContext *fmt_ctx = nullptr;
    AVCodec *decoder = nullptr;
    int streamIndex = -1;
    /** Sorted by pts */
    std::vector<PacketIndexEntry> packetIndex;
    void dispose();
    FileIO &getFileIO();

  public:
    Video(std::string url);
    Video(Video &&other) noexcept : ioContext(nullptr, nullptr) { *this = std::move(other); }
    Video &operator=(Video &&other) noexcept;
    Video(const Video &) = delete;
    Video &operator=(const Video &) = delete;
    ~Video() { this->dispose(); }

    void sleep();
    void weakUp();
    bool isSleeping();

    void getBatch(const std::vector<int> &frameIndices);
};

class VideoLoader {
  public:
    Video addVideoFile(std::string url);
};

} // namespace videoloader
} // namespace huww
