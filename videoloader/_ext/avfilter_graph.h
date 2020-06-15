#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}

#include "av_utils.h"

namespace huww {
namespace videoloader {

struct avfilter_graph_deleter {
    void operator()(::AVFilterGraph *i) { avfilter_graph_free(&i); }
};

using ffavgraph_ptr = std::unique_ptr<::AVFilterGraph, avfilter_graph_deleter>;

class avfilter_graph {
  private:
    ffavgraph_ptr graph;
    AVFilterContext *buffersrc_ctx;
    AVFilterContext *buffersink_ctx;
    avframe_ptr filtered_frame;

  public:
    avfilter_graph(AVCodecContext &decode_context, AVRational time_base);
    AVFrame *process_frame(AVFrame *src);
};

} // namespace videoloader
} // namespace huww
