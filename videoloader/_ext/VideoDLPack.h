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

class DLPackPoolContext;

class DLPackPool {
    std::multimap<size_t, DLManagedTensor *> pool;
    DLPackPoolContext *context;

    friend class DLPackPoolContext;
    void returnPackInternal(DLManagedTensor *);

  public:
    DLPackPool();
    ~DLPackPool();
    DLPackPool(const DLPackPool &) = delete;
    VideoDLPack::ptr get(size_t size);
    void returnPack(VideoDLPack::ptr &&pack);
};

class VideoDLPackBuilder {
    int numFrames;
    VideoDLPack::ptr dlTensor;
    DLPackPool *pool;

  public:
    explicit VideoDLPackBuilder(int numFrames, DLPackPool *pool = nullptr);
    void copyFromFrame(AVFrame *frame, int index);

    VideoDLPack::ptr result() noexcept { return std::move(dlTensor); }
};

} // namespace videoloader
} // namespace huww
