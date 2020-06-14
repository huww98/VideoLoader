#include "videoloader.h"

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

Video VideoLoader::addVideoFile(std::string url) { return Video(url); }
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, void (*)(AVCodecContext *)>;

auto new_AVCodecContext(const AVCodec *codec) {
    return AVCodecContextPtr(CHECK_AV(avcodec_alloc_context3(codec), "alloc AVCodecContext failed"),
                             [](AVCodecContext *c) { avcodec_free_context(&c); });
}

using AVPacketPtr = std::unique_ptr<AVPacket, void (*)(AVPacket *)>;

auto allocAVPacket() {
    return AVPacketPtr(CHECK_AV(av_packet_alloc(), "alloc AVPacket failed"),
                       [](AVPacket *p) { av_packet_free(&p); });
}

Video::Video(std::string url) : format(url) {
    auto fmt_ctx = format.formatContext();
    CHECK_AV(avformat_find_stream_info(fmt_ctx, nullptr), "find stream info failed");

    streamIndex =
        CHECK_AV(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &this->decoder, 0),
                 "Unable to find video stream for \"" << url << "\"");

    auto packet = allocAVPacket();

    int lastKeyFrameIndex = -1;
    int nextPacketIndex = 0;
    while (true) {
        int ret = av_read_frame(fmt_ctx, packet.get());
        if (ret == AVERROR_EOF) {
            break;
        }
        CHECK_AV(ret, "read frame failed");
        if (packet->stream_index != streamIndex) {
            continue;
        }
        if (packet->flags & AV_PKT_FLAG_KEY) {
            lastKeyFrameIndex = packetIndex.size();
        }
        packetIndex.push_back({
            .pts = packet->pts,
            .keyFrameIndex = lastKeyFrameIndex,
            .packetIndex = nextPacketIndex++,
        });
        av_packet_unref(packet.get());
    }
    CHECK_AV(av_seek_frame(fmt_ctx, streamIndex, 0, AVSEEK_FLAG_BACKWARD), "failed to seek back");
    std::sort(packetIndex.begin(), packetIndex.end(),
              [](auto &a, auto &b) { return a.pts < b.pts; });
    {
        // Adjust key frame index. although I think key frame position should
        // not change during sorting.
        std::vector<int> sortMap(packetIndex.size());
        for (size_t i = 0; i < packetIndex.size(); i++) {
            sortMap[packetIndex[i].packetIndex] = i;
        }
        for (auto &entry : packetIndex) {
            entry.keyFrameIndex = sortMap[entry.keyFrameIndex];
        }
    }
}

class VideoPacketScheduler {
  private:
    struct ScheduleEntry {
        std::unordered_set<int64_t> neededPts;
        int lastPacketIndex;
        /** Used to seek */
        int64_t keyFramePts;
    };
    AVFormatContext *fmt_ctx;
    int streamIndex;
    AVPacketPtr packet;
    bool packetConsumed = true;
    bool _finished = false;

    /** Map from key frame index */
    std::map<int, ScheduleEntry> schedule;
    decltype(schedule.begin()) currentSchedule;

    void seek() {
        if (currentSchedule == schedule.end()) {
            return;
        }
        CHECK_AV(av_seek_frame(fmt_ctx, streamIndex, currentSchedule->second.keyFramePts,
                               AVSEEK_FLAG_BACKWARD),
                 "failed to seek");
    }

  public:
    VideoPacketScheduler(const std::vector<size_t> &frameIndicesRequested,
                         const std::vector<PacketIndexEntry> &index, AVFormatContext *fmt_ctx,
                         int streamIndex)
        : fmt_ctx(fmt_ctx), streamIndex(streamIndex), packet(allocAVPacket()) {
        for (size_t f : frameIndicesRequested) {
            auto &pktIndex = index[f];
            auto &entry = schedule[pktIndex.keyFrameIndex];
            entry.keyFramePts = index[pktIndex.keyFrameIndex].pts;
            entry.neededPts.insert(pktIndex.pts);
            entry.lastPacketIndex = std::max(entry.lastPacketIndex, pktIndex.packetIndex);
        }
        if (!schedule.empty()) {
            // Merge adjecent schedule.
            for (auto it = std::next(schedule.begin()); it != schedule.end();) {
                auto &previousEntry = std::prev(it)->second;
                auto keyPktIdx = index[it->first].packetIndex;
                if (keyPktIdx - 1 == previousEntry.lastPacketIndex) {
                    previousEntry.neededPts.merge(it->second.neededPts);
                    previousEntry.lastPacketIndex = it->second.lastPacketIndex;
                    it = schedule.erase(it);
                } else {
                    ++it;
                }
            }
        }
        currentSchedule = schedule.begin();
        seek();
    }

    bool finished() { return _finished; }
    AVPacket *next() {
        if (_finished) {
            throw std::runtime_error("No more packet.");
        }
        if (!packetConsumed) {
            return packet.get();
        }
        packetConsumed = false;
        if (currentSchedule == schedule.end()) {
            // Signal EOF, return empty packet.
            return packet.get();
        }

        CHECK_AV(av_read_frame(fmt_ctx, packet.get()), "read frame failed");

        auto &neededPts = currentSchedule->second.neededPts;
        auto ptsIt = neededPts.find(packet->pts);
        if (ptsIt == neededPts.end()) {
            packet->flags |= AV_PKT_FLAG_DISCARD;
        } else {
            neededPts.erase(ptsIt);
            if (neededPts.empty()) {
                currentSchedule++;
                seek();
            }
        }
        return packet.get();
    }
    void consume() {
        if (packetConsumed) {
            throw std::runtime_error("packet consumed twice.");
        }
        packetConsumed = true;
        if (packet->size == 0) {
            // This packet is EOF
            this->_finished = true;
        } else {
            av_packet_unref(packet.get());
        }
    }
};

struct FrameRequest {
    size_t requestIndex;
    int64_t pts;
};

VideoDLPack::ptr Video::getBatch(const std::vector<size_t> &frameIndices) {
    this->weakUp();

    std::vector<FrameRequest> request(frameIndices.size());
    for (size_t i = 0; i < frameIndices.size(); i++) {
        auto frameIndex = frameIndices[i];
        if (frameIndex >= this->packetIndex.size()) {
            std::ostringstream msg;
            msg << "Specified frame index " << frameIndex << " is out of range";
            throw std::out_of_range(msg.str());
        }
        request[i].requestIndex = i;
        request[i].pts = this->packetIndex[frameIndex].pts;
    }
    std::sort(request.begin(), request.end(),
              [](FrameRequest &a, FrameRequest &b) { return a.pts < b.pts; });

    auto fmt_ctx = format.formatContext();

    VideoPacketScheduler packetScheduler(frameIndices, packetIndex, fmt_ctx, this->streamIndex);

    auto decodeContext = new_AVCodecContext(decoder);

    CHECK_AV(avcodec_parameters_to_context(decodeContext.get(), currentStream().codecpar),
             "failed to set codec parameters");
    CHECK_AV(avcodec_open2(decodeContext.get(), decoder, nullptr), "open decoder failed");

    AVFilterGraph fg(*decodeContext.get(), currentStream().time_base);
    VideoDLPackBuilder packBuilder(request.size());
    auto nextRequest = request.cbegin();

    auto frame = allocAVFrame();

    bool eof = false;
    while (!eof) {
        if (!packetScheduler.finished()) {
            auto packet = packetScheduler.next();
            CHECK_AV(avcodec_send_packet(decodeContext.get(), packet),
                     "send packet to decoder failed");
            SPDLOG_TRACE("Send packet DTS {} PTS {}", packet->dts, packet->pts);
            packetScheduler.consume();
        }

        while (true) {
            int ret = avcodec_receive_frame(decodeContext.get(), frame.get());
            if (ret == AVERROR(EAGAIN)) {
                break;
            }
            if (ret == AVERROR_EOF) {
                eof = true;
                break;
            }
            CHECK_AV(ret, "receive frame from decoder failed");
            SPDLOG_TRACE("Received frame PTS {}", frame->pts);

            if (frame->pts == nextRequest->pts) {
                auto filteredFrame = fg.processFrame(frame.get());
                SPDLOG_TRACE("Filtered frame PTS {}", filteredFrame->pts);
                do {
                    packBuilder.copyFromFrame(filteredFrame, nextRequest->requestIndex);
                    SPDLOG_TRACE("Copied to index {}", nextRequest->requestIndex);
                    nextRequest++;
                } while (nextRequest != request.cend() && filteredFrame->pts == nextRequest->pts);
                av_frame_unref(filteredFrame);
                if (nextRequest == request.cend()) {
                    eof = true;
                    break;
                }
            }
        }
    }
    assert(nextRequest == request.cend());
    return packBuilder.result();
}

void Video::sleep() { this->format.sleep(); }

void Video::weakUp() { this->format.weakUp(); }

bool Video::isSleeping() { return this->format.isSleeping(); }

AVStream &Video::currentStream() noexcept {
    return *this->format.formatContext()->streams[this->streamIndex];
}

AVRational Video::averageFrameRate() noexcept { return this->currentStream().avg_frame_rate; }

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
