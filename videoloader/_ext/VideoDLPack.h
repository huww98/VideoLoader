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
    static void free(DLManagedTensor *);

  private:
    int numFrames;
    std::unique_ptr<DLManagedTensor, decltype(&VideoDLPack::free)> dlTensor;

  public:
    VideoDLPack(int numFrames);
    void copyFromFrame(AVFrame *frame, int index);

    DLManagedTensor *release() noexcept { return dlTensor.release(); }
};

} // namespace videoloader
} // namespace huww
