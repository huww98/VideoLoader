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

struct dlpack_deleter {
    void operator()(DLManagedTensor *dlpack) { dlpack->deleter(dlpack); }
};

class video_dlpack {
  public:
    static void free(DLManagedTensor *);
    using ptr = std::unique_ptr<DLManagedTensor, dlpack_deleter>;
    static ptr alloc(size_t size);
};

class dlpack_pool_context;

class dlpack_pool {
    std::multimap<size_t, DLManagedTensor *> pool;
    dlpack_pool_context *context;

    friend class dlpack_pool_context;
    void return_pack_internal(DLManagedTensor *);

  public:
    dlpack_pool();
    ~dlpack_pool();
    dlpack_pool(const dlpack_pool &) = delete;
    video_dlpack::ptr get(size_t size);
    void return_pack(video_dlpack::ptr &&pack);
};

class video_dlpack_builder {
    int num_frames;
    video_dlpack::ptr dlpack;
    dlpack_pool *pool;

  public:
    explicit video_dlpack_builder(int num_frames, dlpack_pool *pool = nullptr);
    void copy_from_frame(AVFrame *frame, int index);

    video_dlpack::ptr result() noexcept { return std::move(dlpack); }
};

} // namespace videoloader
} // namespace huww
