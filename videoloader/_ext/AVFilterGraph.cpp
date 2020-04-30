#include "AVFilterGraph.h"

#include <assert.h>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include "av_utils.h"

namespace huww {
namespace videoloader {

auto allocAVFilterGraph() {
    return FFGraphPtr(
        CHECK_AV(avfilter_graph_alloc(), "failed to alloc AVFilterGraph"),
        [](::AVFilterGraph *&&i) { avfilter_graph_free(&i); });
}

AVFilterGraph::AVFilterGraph(AVCodecContext &decodeContext, AVRational timeBase)
    : graph(allocAVFilterGraph()), filteredFrame(allocAVFrame()) {
    auto buffersrc = avfilter_get_by_name("buffer");
    auto buffersink = avfilter_get_by_name("buffersink");

    std::ostringstream args;
    args << "video_size=" << decodeContext.width << "x" << decodeContext.height;
    args << ":pix_fmt=" << decodeContext.pix_fmt;
    args << ":time_base=" << timeBase.num << "/" << timeBase.den;

    CHECK_AV(avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                          args.str().c_str(), nullptr,
                                          graph.get()),
             "failed to create buffer source");
    CHECK_AV(avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                          nullptr, nullptr, graph.get()),
             "failed to create buffer sink");

    static AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE};
    CHECK_AV(av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                                 AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN),
             "failed to set output pixel format");

    auto inputs = avfilter_inout_alloc();
    auto outputs = avfilter_inout_alloc();

    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    CHECK_AV(avfilter_graph_parse_ptr(graph.get(), "null", &inputs, &outputs,
                                      nullptr),
             "avfilter_graph_parse_ptr failed");

    CHECK_AV(avfilter_graph_config(graph.get(), nullptr),
             "avfilter_graph_config failed");

    assert(inputs == nullptr && outputs == nullptr);
}

AVFrame *AVFilterGraph::processFrame(AVFrame *src) {
    // assume one frame output per input frame.
    CHECK_AV(av_buffersrc_add_frame(this->buffersrc_ctx, src),
             "Error while feeding the filtergraph");
    CHECK_AV(av_buffersink_get_frame(this->buffersink_ctx,
                                     this->filteredFrame.get()),
             "Error while getting frame from the filtergraph");
    return this->filteredFrame.get();
}

} // namespace videoloader
} // namespace huww
