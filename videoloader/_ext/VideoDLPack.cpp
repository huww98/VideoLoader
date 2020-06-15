#include "VideoDLPack.h"

#include <array>
#include <assert.h>
#include <spdlog/spdlog.h>

namespace huww {
namespace videoloader {

VideoDLPackBuilder::VideoDLPackBuilder(int numFrames, DLPackPool *pool)
    : numFrames(numFrames), dlTensor(nullptr), pool(pool) {}

void VideoDLPackBuilder::copyFromFrame(AVFrame *frame, int index) {
    assert(index < numFrames);
    assert(frame->format == AVPixelFormat::AV_PIX_FMT_RGB24);
    auto linesize = frame->linesize[0];
    auto frameSize = linesize * frame->height;

    if (!dlTensor) {
        auto size = frameSize * numFrames;
        if (this->pool) {
            dlTensor = this->pool->get(size);
        } else {
            dlTensor = VideoDLPack::alloc(size);
        }
        std::array<int64_t, 4> shape = {numFrames, frame->width, frame->height, 3};
        std::array<int64_t, 4> strides = {frameSize, 3, linesize, 1};
        auto &dl = dlTensor->dl_tensor;
        std::copy(shape.begin(), shape.end(), dl.shape);
        std::copy(strides.begin(), strides.end(), dl.strides);
    }
    auto &dl = dlTensor->dl_tensor;
    assert(linesize == dl.strides[2]);
    assert(frame->width == dl.shape[1]);
    assert(frame->height == dl.shape[2]);

    auto dest = static_cast<uint8_t *>(dl.data) + frameSize * index;
    memcpy(dest, frame->data[0], frameSize);
}

auto VideoDLPack::alloc(size_t size) -> VideoDLPack::ptr {
    return VideoDLPack::ptr(new DLManagedTensor{
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
        .deleter = &VideoDLPack::free,
    });
}

void VideoDLPack::free(DLManagedTensor *dlTensor) {
    auto &dl = dlTensor->dl_tensor;
    delete[] dl.shape;
    delete[] dl.strides;
    ::free(dl.data);
    delete dlTensor;
}

struct PooledDLPackState {
    size_t size;
    DLPackPoolContext &poolContext;
};

void freePooledDLPack(DLManagedTensor *dlTensor) {
    delete static_cast<PooledDLPackState *>(dlTensor->manager_ctx);
    VideoDLPack::free(dlTensor);
}

class DLPackPoolContext {
    std::mutex m;
    long handedOutPack = 0;
    bool poolAlive = true;
    DLPackPool *pool;

    friend class DLPackPool;

  public:
    DLPackPoolContext(DLPackPool *pool) : pool(pool) {}
    void returnPack(DLManagedTensor *dlTensor) {
        bool deleteContext;
        {
            std::lock_guard lk(m);
            handedOutPack--;
            assert(handedOutPack >= 0);
            if (poolAlive) {
                pool->returnPackInternal(dlTensor);
            } else {
                SPDLOG_TRACE("Pool disposed, freeing DLTensor.");
                freePooledDLPack(dlTensor);
            }
            deleteContext = handedOutPack == 0 && !poolAlive;
        }
        if (deleteContext) {
            delete this;
        }
    }
    DLPackPoolContext(const DLPackPoolContext &) = delete;
    DLPackPoolContext(DLPackPoolContext &&) = delete;
};

DLPackPool::DLPackPool() : context(new DLPackPoolContext(this)) {}
DLPackPool::~DLPackPool() {
    bool deleteContext;
    {
        std::lock_guard lk(context->m);
        context->poolAlive = false;
        context->pool = nullptr;
        for (auto &[_, dlTensor] : this->pool) {
            freePooledDLPack(dlTensor);
        }
        deleteContext = context->handedOutPack == 0;
    }
    if (deleteContext) {
        delete context;
    }
}

VideoDLPack::ptr DLPackPool::get(size_t size) {
    std::lock_guard lk(context->m);
    context->handedOutPack++;
    auto it = pool.lower_bound(size);
    if (it != pool.end() && it->first < size * 2) {
        SPDLOG_TRACE("Reusing DLTensor of size {} for request of size {}", it->first, size);
        auto reusedTensor = it->second;
        pool.erase(it);
        return VideoDLPack::ptr(reusedTensor);
    }
    SPDLOG_TRACE("Allocating new DLTensor of size {}", size);
    auto newTensor = VideoDLPack::alloc(size);
    newTensor->manager_ctx = new PooledDLPackState{
        .size = size,
        .poolContext = *this->context,
    };
    newTensor->deleter = [](DLManagedTensor *dlTensor) {
        auto ctx = static_cast<PooledDLPackState *>(dlTensor->manager_ctx);
        ctx->poolContext.returnPack(dlTensor);
    };
    return newTensor;
}

void DLPackPool::returnPackInternal(DLManagedTensor *dlTensor) {
    auto ctx = static_cast<PooledDLPackState *>(dlTensor->manager_ctx);
    assert(&ctx->poolContext == context);

    auto it = pool.lower_bound(ctx->size);
    pool.insert(it, {ctx->size, dlTensor});
    SPDLOG_TRACE("Returned DLTensor of size {}", ctx->size);
}

void DLPackPool::returnPack(VideoDLPack::ptr &&pack) {
    std::lock_guard lk(context->m);
    this->returnPackInternal(pack.release());
}

} // namespace videoloader
} // namespace huww
