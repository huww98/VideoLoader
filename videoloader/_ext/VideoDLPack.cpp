#include "VideoDLPack.h"

#include <assert.h>

namespace huww {
namespace videoloader {

VideoDLPack::VideoDLPack(int numFrames)
    : numFrames(numFrames), dlTensor(nullptr, [](auto t) { t->deleter(t); }) {}

void VideoDLPack::copyFromFrame(AVFrame *frame, int index) {
    assert(index < numFrames);
    assert(frame->format == AVPixelFormat::AV_PIX_FMT_RGB24);
    auto linesize = frame->linesize[0];
    auto frameSize = linesize * frame->height;

    if (!dlTensor) {
        dlTensor.reset(new DLManagedTensor{
            .dl_tensor =
                {
                    .data = aligned_alloc(64, frameSize * numFrames),
                    .ctx = {.device_type = kDLCPU},
                    .ndim = 4, // frame, width, height, channel
                    .dtype =
                        {
                            .code = kDLUInt,
                            .bits = 8,
                            .lanes = 1,
                        },
                    .shape = new int64_t[4]{numFrames, frame->width,
                                            frame->height, 3},
                    .strides = new int64_t[4]{frameSize, 3, linesize, 1},
                    .byte_offset = 0,
                },
            .manager_ctx = nullptr,
            .deleter = &VideoDLPack::free,
        });
    }
    auto &dl = dlTensor->dl_tensor;
    assert(linesize == dl.strides[2]);
    assert(frame->width == dl.shape[1]);
    assert(frame->height == dl.shape[2]);

    auto dest = static_cast<uint8_t *>(dl.data) + frameSize * index;
    memcpy(dest, frame->data[0], frameSize);
}

void VideoDLPack::free(DLManagedTensor *dlTensor) {
    auto &dl = dlTensor->dl_tensor;
    delete[] dl.shape;
    delete[] dl.strides;
    ::free(dl.data);
    delete dlTensor;
}

} // namespace videoloader
} // namespace huww
