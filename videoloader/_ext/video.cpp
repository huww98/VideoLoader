#include "video.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "av_utils.h"

namespace huww {
namespace videoloader {

struct avcodec_context_deleter {
    void operator()(AVCodecContext *c) { avcodec_free_context(&c); }
};
using ffavcodec_context_ptr = std::unique_ptr<AVCodecContext, avcodec_context_deleter>;

auto new_avcodec_context(const AVCodec *codec) {
    return ffavcodec_context_ptr(
        CHECK_AV(avcodec_alloc_context3(codec), "alloc AVCodecContext failed"));
}

struct avpacket_deleter {
    void operator()(AVPacket *p) { av_packet_free(&p); }
};

using avpacket_ptr = std::unique_ptr<AVPacket, avpacket_deleter>;

auto new_avpacket() { return avpacket_ptr(CHECK_AV(av_packet_alloc(), "alloc AVPacket failed")); }

video::video(std::string url) : video(file_io::file_spec{.path = url}) {}

video::video(const file_io::file_spec &spec) : format(spec) {
    auto fmt_ctx = format.format_context();
    CHECK_AV(avformat_find_stream_info(fmt_ctx, nullptr), "find stream info failed");

    stream_index =
        CHECK_AV(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &this->decoder, 0),
                 "Unable to find video stream for \"" << spec.path << "\"");

    auto packet = new_avpacket();

    int last_key_frame_index = -1;
    int next_packet_index = 0;
    while (true) {
        int ret = av_read_frame(fmt_ctx, packet.get());
        if (ret == AVERROR_EOF) {
            break;
        }
        CHECK_AV(ret, "read frame failed");
        if (packet->stream_index != stream_index) {
            continue;
        }
        if (packet->flags & AV_PKT_FLAG_KEY) {
            last_key_frame_index = packet_index.size();
        }
        // First frame should be a key frame
        assert(last_key_frame_index >= 0);
        packet_index.push_back({
            .pts = packet->pts,
            .key_frame_index = last_key_frame_index,
            .packet_index = next_packet_index++,
        });
        av_packet_unref(packet.get());
    }
    CHECK_AV(av_seek_frame(fmt_ctx, stream_index, 0, AVSEEK_FLAG_BACKWARD), "failed to seek back");
    std::sort(packet_index.begin(), packet_index.end(),
              [](auto &a, auto &b) { return a.pts < b.pts; });
    {
        // Adjust key frame index. although I think key frame position should
        // not change during sorting.
        std::vector<int> sort_map(packet_index.size());
        for (size_t i = 0; i < packet_index.size(); i++) {
            sort_map[packet_index[i].packet_index] = i;
        }
        for (auto &entry : packet_index) {
            entry.key_frame_index = sort_map[entry.key_frame_index];
        }
    }
}

class video_packet_scheduler {
  private:
    struct schedule_entry {
        std::unordered_set<int64_t> needed_pts;
        int last_packet_index;
        int64_t key_frame_pts; /**< Used to seek */
    };
    AVFormatContext *fmt_ctx;
    int stream_index;
    avpacket_ptr packet;
    bool packet_consumed = true;
    bool _finished = false;

    /** Map from key frame index */
    std::map<int, schedule_entry> schedule;
    decltype(schedule.begin()) current_schedule;

    void seek() {
        if (current_schedule == schedule.end()) {
            return;
        }
        CHECK_AV(av_seek_frame(fmt_ctx, stream_index, current_schedule->second.key_frame_pts,
                               AVSEEK_FLAG_BACKWARD),
                 "failed to seek");
    }

  public:
    video_packet_scheduler(const std::vector<size_t> &frame_indices_requested,
                           const std::vector<packet_index_entry> &index, AVFormatContext *fmt_ctx,
                           int stream_index)
        : fmt_ctx(fmt_ctx), stream_index(stream_index), packet(new_avpacket()) {
        for (size_t f : frame_indices_requested) {
            auto &pkt_index = index[f];
            auto &entry = schedule[pkt_index.key_frame_index];
            entry.key_frame_pts = index[pkt_index.key_frame_index].pts;
            entry.needed_pts.insert(pkt_index.pts);
            entry.last_packet_index = std::max(entry.last_packet_index, pkt_index.packet_index);
        }
        if (!schedule.empty()) {
            // Merge adjecent schedule.
            for (auto it = std::next(schedule.begin()); it != schedule.end();) {
                auto &previous_entry = std::prev(it)->second;
                auto key_pkt_idx = index[it->first].packet_index;
                if (key_pkt_idx - 1 == previous_entry.last_packet_index) {
                    previous_entry.needed_pts.merge(it->second.needed_pts);
                    previous_entry.last_packet_index = it->second.last_packet_index;
                    it = schedule.erase(it);
                } else {
                    ++it;
                }
            }
        }
        current_schedule = schedule.begin();
        seek();
    }

    bool finished() { return _finished; }
    AVPacket *next() {
        if (_finished) {
            throw std::logic_error("No more packet.");
        }
        if (!packet_consumed) {
            return packet.get();
        }
        packet_consumed = false;
        if (current_schedule == schedule.end()) {
            // Signal EOF, return empty packet.
            return packet.get();
        }

        CHECK_AV(av_read_frame(fmt_ctx, packet.get()), "read frame failed");

        auto &needed_pts = current_schedule->second.needed_pts;
        auto pts_it = needed_pts.find(packet->pts);
        if (pts_it == needed_pts.end()) {
            packet->flags |= AV_PKT_FLAG_DISCARD;
        } else {
            needed_pts.erase(pts_it);
            if (needed_pts.empty()) {
                current_schedule++;
                seek();
            }
        }
        return packet.get();
    }
    void consume() {
        if (packet_consumed) {
            throw std::runtime_error("packet consumed twice.");
        }
        packet_consumed = true;
        if (packet->size == 0) {
            // This packet is EOF
            this->_finished = true;
        } else {
            av_packet_unref(packet.get());
        }
    }
};

struct frame_request {
    size_t request_index;
    int64_t pts;
};

video_dlpack::ptr video::get_batch(const std::vector<size_t> &frame_indices, dlpack_pool *pool) {
    this->wake_up();

    std::vector<frame_request> request(frame_indices.size());
    for (size_t i = 0; i < frame_indices.size(); i++) {
        auto frame_index = frame_indices[i];
        if (frame_index >= this->packet_index.size()) {
            std::ostringstream msg;
            msg << "Specified frame index " << frame_index << " is out of range";
            throw std::out_of_range(msg.str());
        }
        request[i].request_index = i;
        request[i].pts = this->packet_index[frame_index].pts;
    }
    std::sort(request.begin(), request.end(),
              [](frame_request &a, frame_request &b) { return a.pts < b.pts; });

    auto fmt_ctx = format.format_context();

    video_packet_scheduler packet_scheduler(frame_indices, packet_index, fmt_ctx,
                                            this->stream_index);

    auto decode_context = new_avcodec_context(decoder);

    CHECK_AV(avcodec_parameters_to_context(decode_context.get(), current_stream().codecpar),
             "failed to set codec parameters");
    CHECK_AV(avcodec_open2(decode_context.get(), decoder, nullptr), "open decoder failed");

    avfilter_graph fg(*decode_context.get(), current_stream().time_base);
    video_dlpack_builder pack_builder(request.size(), pool);
    auto next_request = request.cbegin();

    auto frame = new_avframe();

    bool eof = false;
    while (!eof) {
        if (!packet_scheduler.finished()) {
            auto packet = packet_scheduler.next();
            CHECK_AV(avcodec_send_packet(decode_context.get(), packet),
                     "send packet to decoder failed");
            SPDLOG_TRACE("Send packet DTS {} PTS {}", packet->dts, packet->pts);
            packet_scheduler.consume();
        }

        while (true) {
            int ret = avcodec_receive_frame(decode_context.get(), frame.get());
            if (ret == AVERROR(EAGAIN)) {
                break;
            }
            if (ret == AVERROR_EOF) {
                eof = true;
                break;
            }
            CHECK_AV(ret, "receive frame from decoder failed");
            SPDLOG_TRACE("Received frame PTS {}", frame->pts);

            if (frame->pts == next_request->pts) {
                auto filtered_frame = fg.process_frame(frame.get());
                SPDLOG_TRACE("Filtered frame PTS {}", filtered_frame->pts);
                do {
                    pack_builder.copy_from_frame(filtered_frame, next_request->request_index);
                    SPDLOG_TRACE("Copied to index {}", next_request->request_index);
                    next_request++;
                } while (next_request != request.cend() &&
                         filtered_frame->pts == next_request->pts);
                av_frame_unref(filtered_frame);
                if (next_request == request.cend()) {
                    eof = true;
                    break;
                }
            }
        }
    }
    assert(next_request == request.cend());
    return pack_builder.result();
}

void video::sleep() { this->format.sleep(); }

void video::wake_up() { this->format.wake_up(); }

bool video::is_sleeping() { return this->format.is_sleeping(); }

AVStream &video::current_stream() noexcept {
    return *this->format.format_context()->streams[this->stream_index];
}

AVRational video::average_frame_rate() noexcept { return this->current_stream().avg_frame_rate; }

void init() {
#if (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100))
    av_register_all();
#endif
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
    avfilter_register_all();
#endif
}

} // namespace videoloader
} // namespace huww
