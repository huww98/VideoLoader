#pragma once

#include <memory>

#include "third_party/dlpack.h"

extern "C" {
#include <libavutil/frame.h>
}

namespace huww {
namespace videoloader {

class VideoDLPack {
  public:
    static void free(DLTensor *);

  private:
    int numFrames;
    std::unique_ptr<DLTensor, decltype(&VideoDLPack::free)> dlTensor;

  public:
    VideoDLPack(int numFrames);
    void addFrame(AVFrame *frame, int index);

    DLTensor *release() noexcept { return dlTensor.release(); }
};

} // namespace videoloader
} // namespace huww
