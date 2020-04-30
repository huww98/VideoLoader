#include "DLPack.h"

#include <assert.h>

namespace huww {
namespace videoloader {

VideoDLPack::VideoDLPack(int numFrames)
    : numFrames(numFrames), dlTensor(nullptr, &VideoDLPack::free) {}

void VideoDLPack::addFrame(AVFrame *frame, int index) {
    assert(index < numFrames);
    assert(frame->format == AVPixelFormat::AV_PIX_FMT_RGB24);
    auto linesize = frame->linesize[0];
    auto frameSize = linesize * frame->height;

    if (!dlTensor) {
        dlTensor.reset(new DLTensor{
            .data = new uint8_t[frameSize * numFrames],
            .ctx = {.device_type = kDLCPU},
            .ndim = 4, // frame, width, height, channel
            .dtype =
                {
                    .code = kDLUInt,
                    .bits = 8,
                    .lanes = 1,
                },
            .shape = new int64_t[4]{numFrames, frame->width, frame->height, 3},
            .strides = new int64_t[4]{linesize * frame->width, linesize, 3, 1},
            .byte_offset = 0,
        });
    }
    assert(linesize == dlTensor->strides[1]);
    assert(frame->width == dlTensor->shape[1]);
    assert(frame->height == dlTensor->shape[2]);

    auto dest = static_cast<uint8_t *>(dlTensor->data) + frameSize * index;
    memcpy(dest, frame->data[0], frameSize);
}

void VideoDLPack::free(DLTensor *dlTensor) {
    delete[] dlTensor->shape;
    delete[] dlTensor->strides;
    delete[] static_cast<uint8_t *>(dlTensor->data);
    delete dlTensor;
}

} // namespace videoloader
} // namespace huww
