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

    graph->thread_type = 0;
    avfilter_graph_set_auto_convert(graph.get(), AVFILTER_AUTO_CONVERT_NONE);

    auto buffersrc = avfilter_get_by_name("buffer");
    auto buffersink = avfilter_get_by_name("buffersink");

    buffersrc_ctx =
        CHECK_AV(avfilter_graph_alloc_filter(graph.get(), buffersrc, "in"),
                 "failed to alloc buffer source");
    AVBufferSrcParameters srcParams{
        .format = decodeContext.pix_fmt,
        .time_base = timeBase,
        .width = decodeContext.width,
        .height = decodeContext.height,
        .sample_aspect_ratio = decodeContext.sample_aspect_ratio,
    };
    CHECK_AV(av_buffersrc_parameters_set(buffersrc_ctx, &srcParams),
             "failed to set buffer source parameters");
    CHECK_AV(avfilter_init_str(buffersrc_ctx, nullptr),
             "failed to initialize buffer source");

    CHECK_AV(avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                          nullptr, nullptr, graph.get()),
             "failed to create buffer sink");

    static AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE};
    CHECK_AV(av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                                 AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN),
             "failed to set output pixel format");

    auto scale = avfilter_get_by_name("scale");
    AVFilterContext *scale_ctx;
    CHECK_AV(avfilter_graph_create_filter(&scale_ctx, scale, "scale", nullptr,
                                          nullptr, graph.get()),
             "create scale filter failed");

    CHECK_AV(avfilter_link(buffersrc_ctx, 0, scale_ctx, 0),
             "link buffersrc failed");
    CHECK_AV(avfilter_link(scale_ctx, 0, buffersink_ctx, 0),
             "link buffersink failed");

    CHECK_AV(avfilter_graph_config(graph.get(), nullptr),
             "avfilter_graph_config failed");
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
