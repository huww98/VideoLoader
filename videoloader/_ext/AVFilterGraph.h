#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}

#include "av_utils.h"

namespace huww {
namespace videoloader {
namespace {
using FFGraphPtr =
    std::unique_ptr<::AVFilterGraph, void (*)(::AVFilterGraph *&&)>;
}

class AVFilterGraph {
  private:
    FFGraphPtr graph;
    AVFilterContext *buffersrc_ctx;
    AVFilterContext *buffersink_ctx;
    AVFramePtr filteredFrame;

  public:
    AVFilterGraph(AVCodecContext &decodeContext, AVRational timeBase);
    AVFrame *processFrame(AVFrame *src);
};

} // namespace videoloader
} // namespace huww
