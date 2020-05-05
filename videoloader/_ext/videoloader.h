#pragma once

#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
    #include <libavcodec/avcodec.h>
}

#include "AVFormat.h"
#include "AVFilterGraph.h"
#include "VideoDLPack.h"

namespace huww {
namespace videoloader {

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
    Video(std::string url);

    void sleep();
    void weakUp();
    bool isSleeping();

    size_t numFrames() noexcept { return packetIndex.size(); }
    AVRational averageFrameRate() noexcept;

    VideoDLPack getBatch(const std::vector<int> &frameIndices);
};

class VideoLoader {
  public:
    Video addVideoFile(std::string url);
};

} // namespace videoloader
} // namespace huww
