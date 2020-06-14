#pragma once

#include <map>
#include <memory>
#include <mutex>

#include "third_party/dlpack.h"

extern "C" {
#include <libavutil/frame.h>
}

namespace huww {
namespace videoloader {

struct DLPackDeleter {
    void operator()(DLManagedTensor *dlTensor) { dlTensor->deleter(dlTensor); }
};

class VideoDLPack {
  public:
    static void free(DLManagedTensor *);
    using ptr = std::unique_ptr<DLManagedTensor, DLPackDeleter>;
    static ptr alloc(size_t size);
};

class VideoDLPackBuilder {
    int numFrames;
    VideoDLPack::ptr dlTensor;

  public:
    explicit VideoDLPackBuilder(int numFrames);
    void copyFromFrame(AVFrame *frame, int index);

    VideoDLPack::ptr result() noexcept { return std::move(dlTensor); }
};

class DLPackPoolContext;

class DLPackPool {
    std::multimap<size_t, DLManagedTensor *> pool;
    DLPackPoolContext *context;

  public:
    DLPackPool();
    ~DLPackPool();
    DLPackPool(const DLPackPool &) = delete;
    VideoDLPack::ptr get(size_t size);
    void returnPack(VideoDLPack::ptr &&pack);
};

} // namespace videoloader
} // namespace huww
