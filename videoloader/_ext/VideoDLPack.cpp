#include "VideoDLPack.h"

#include <array>
#include <assert.h>
#include <spdlog/spdlog.h>

namespace huww {
namespace videoloader {

video_dlpack_builder::video_dlpack_builder(int num_frames, dlpack_pool *pool)
    : num_frames(num_frames), dlpack(nullptr), pool(pool) {}

void video_dlpack_builder::copy_from_frame(AVFrame *frame, int index) {
    assert(index < num_frames);
    assert(frame->format == AVPixelFormat::AV_PIX_FMT_RGB24);
    auto linesize = frame->linesize[0];
    auto frame_size = linesize * frame->height;

    if (!dlpack) {
        auto size = frame_size * num_frames;
        if (this->pool) {
            dlpack = this->pool->get(size);
        } else {
            dlpack = video_dlpack::alloc(size);
        }
        std::array<int64_t, 4> shape = {num_frames, frame->width, frame->height, 3};
        std::array<int64_t, 4> strides = {frame_size, 3, linesize, 1};
        auto &dl = dlpack->dl_tensor;
        std::copy(shape.begin(), shape.end(), dl.shape);
        std::copy(strides.begin(), strides.end(), dl.strides);
    }
    auto &dl = dlpack->dl_tensor;
    assert(linesize == dl.strides[2]);
    assert(frame->width == dl.shape[1]);
    assert(frame->height == dl.shape[2]);

    auto dest = static_cast<uint8_t *>(dl.data) + frame_size * index;
    memcpy(dest, frame->data[0], frame_size);
}

auto video_dlpack::alloc(size_t size) -> video_dlpack::ptr {
    return video_dlpack::ptr(new DLManagedTensor{
        .dl_tensor =
            {
                .data = aligned_alloc(64, size),
                .ctx = {.device_type = kDLCPU},
                .ndim = 4, // frame, width, height, channel
                .dtype =
                    {
                        .code = kDLUInt,
                        .bits = 8,
                        .lanes = 1,
                    },
                .shape = new int64_t[4],
                .strides = new int64_t[4],
                .byte_offset = 0,
            },
        .manager_ctx = nullptr,
        .deleter = &video_dlpack::free,
    });
}

void video_dlpack::free(DLManagedTensor *dlpack) {
    auto &dl = dlpack->dl_tensor;
    delete[] dl.shape;
    delete[] dl.strides;
    ::free(dl.data);
    delete dlpack;
}

struct pooled_dlpack_state {
    size_t size;
    dlpack_pool_context &pool_context;
};

void free_pooled_dlpack(DLManagedTensor *dlpack) {
    delete static_cast<pooled_dlpack_state *>(dlpack->manager_ctx);
    video_dlpack::free(dlpack);
}

class dlpack_pool_context {
    std::mutex m;
    long num_handed_out_pack = 0;
    bool pool_alive = true;
    dlpack_pool *pool;

    friend class dlpack_pool;

  public:
    dlpack_pool_context(dlpack_pool *pool) : pool(pool) {}
    void return_pack(DLManagedTensor *dlpack) {
        bool delete_context;
        {
            std::lock_guard lk(m);
            num_handed_out_pack--;
            assert(num_handed_out_pack >= 0);
            if (pool_alive) {
                pool->return_pack_internal(dlpack);
            } else {
                SPDLOG_TRACE("Pool disposed, freeing DLTensor.");
                free_pooled_dlpack(dlpack);
            }
            delete_context = num_handed_out_pack == 0 && !pool_alive;
        }
        if (delete_context) {
            delete this;
        }
    }
    dlpack_pool_context(const dlpack_pool_context &) = delete;
    dlpack_pool_context(dlpack_pool_context &&) = delete;
};

dlpack_pool::dlpack_pool() : context(new dlpack_pool_context(this)) {}
dlpack_pool::~dlpack_pool() {
    bool delete_context;
    {
        std::lock_guard lk(context->m);
        context->pool_alive = false;
        context->pool = nullptr;
        for (auto &[_, dlpack] : this->pool) {
            free_pooled_dlpack(dlpack);
        }
        delete_context = context->num_handed_out_pack == 0;
    }
    if (delete_context) {
        delete context;
    }
}

video_dlpack::ptr dlpack_pool::get(size_t size) {
    std::lock_guard lk(context->m);
    context->num_handed_out_pack++;
    auto it = pool.lower_bound(size);
    if (it != pool.end() && it->first < size * 2) {
        SPDLOG_TRACE("Reusing DLTensor of size {} for request of size {}", it->first, size);
        auto reused_tensor = it->second;
        pool.erase(it);
        return video_dlpack::ptr(reused_tensor);
    }
    SPDLOG_TRACE("Allocating new DLTensor of size {}", size);
    auto new_tensor = video_dlpack::alloc(size);
    new_tensor->manager_ctx = new pooled_dlpack_state{
        .size = size,
        .pool_context = *this->context,
    };
    new_tensor->deleter = [](DLManagedTensor *dlpack) {
        auto ctx = static_cast<pooled_dlpack_state *>(dlpack->manager_ctx);
        ctx->pool_context.return_pack(dlpack);
    };
    return new_tensor;
}

void dlpack_pool::return_pack_internal(DLManagedTensor *dlpack) {
    auto ctx = static_cast<pooled_dlpack_state *>(dlpack->manager_ctx);
    assert(&ctx->pool_context == context);

    auto it = pool.lower_bound(ctx->size);
    pool.insert(it, {ctx->size, dlpack});
    SPDLOG_TRACE("Returned DLTensor of size {}", ctx->size);
}

void dlpack_pool::return_pack(video_dlpack::ptr &&pack) {
    std::lock_guard lk(context->m);
    this->return_pack_internal(pack.release());
}

} // namespace videoloader
} // namespace huww
