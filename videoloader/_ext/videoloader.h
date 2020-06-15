#pragma once

#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "AVFilterGraph.h"
#include "AVFormat.h"
#include "VideoDLPack.h"

namespace huww {
namespace videoloader {

void init();

struct PacketIndexEntry {
    int64_t pts;
    int keyFrameIndex;
    int packetIndex;
};

class Video {
  private:
    AVFormat format;
    AVCodec *decoder = nullptr;
    int streamIndex = -1;
    /** Sorted by pts */
    std::vector<PacketIndexEntry> packetIndex;

    AVStream &currentStream() noexcept;

  public:
    explicit Video(std::string url);

    void sleep();
    void weakUp();
    bool isSleeping();

    size_t numFrames() const noexcept { return packetIndex.size(); }
    AVRational averageFrameRate() noexcept;

    VideoDLPack::ptr getBatch(const std::vector<std::size_t> &frameIndices,
                              DLPackPool *pool = nullptr);
};

class VideoLoader {
  public:
    Video addVideoFile(std::string url);
};

} // namespace videoloader
} // namespace huww
