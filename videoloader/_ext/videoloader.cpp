#include "videoloader.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_set>

#include "av_utils.h"

namespace huww {
namespace videoloader {

Video VideoLoader::addVideoFile(std::string url) { return Video(url); }
using AVCodecContextPtr =
    std::unique_ptr<AVCodecContext, void (*)(AVCodecContext *&&)>;

auto new_AVCodecContext(const AVCodec *codec) {
    return AVCodecContextPtr(
        CHECK_AV(avcodec_alloc_context3(codec), "alloc AVCodecContext failed"),
        [](AVCodecContext *&&c) { avcodec_free_context(&c); });
}

using AVPacketPtr = std::unique_ptr<AVPacket, void (*)(AVPacket *&&)>;

auto new_AVPacket() {
    return AVPacketPtr(CHECK_AV(av_packet_alloc(), "alloc AVPacket failed"),
                       [](AVPacket *&&p) { av_packet_free(&p); });
}

using AVFramePtr = std::unique_ptr<AVFrame, void (*)(AVFrame *&&)>;

auto new_AVFrame() {
    return AVFramePtr(CHECK_AV(av_frame_alloc(), "alloc AVFrame failed"),
                      [](AVFrame *&&f) { av_frame_free(&f); });
}

Video::Video(std::string url) : ioContext(newFileIOContext(url)) {
    this->fmt_ctx =
        CHECK_AV(avformat_alloc_context(), "Unable to alloc AVFormatContext");

    // Use custom IO, manage AVIOContext ourself to save memory and other
    // resources.
    this->fmt_ctx->pb = ioContext.get();

    CHECK_AV(avformat_open_input(&this->fmt_ctx, url.c_str(), nullptr, nullptr),
             "Unable to open input \"" << url << "\"");

    CHECK_AV(avformat_find_stream_info(fmt_ctx, nullptr),
             "find stream info failed");

    streamIndex = CHECK_AV(av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1,
                                               -1, &this->decoder, 0),
                           "Unable to find video stream for \"" << url << "\"");

    auto packet = new_AVPacket();

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
    CHECK_AV(av_seek_frame(fmt_ctx, streamIndex, 0, AVSEEK_FLAG_BACKWARD),
             "failed to seek back");
    std::sort(packetIndex.begin(), packetIndex.end(),
              [](auto &a, auto &b) { return a.pts < b.pts; });
    {
        // Adjust key frame index. although I think key frame position should
        // not change during sorting.
        std::vector<int> sortMap(packetIndex.size());
        for (int i = 0; i < packetIndex.size(); i++) {
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
        CHECK_AV(av_seek_frame(fmt_ctx, streamIndex,
                               currentSchedule->second.keyFramePts,
                               AVSEEK_FLAG_BACKWARD),
                 "failed to seek");
    }

  public:
    VideoPacketScheduler(const std::vector<int> &frameIndicesRequested,
                         const std::vector<PacketIndexEntry> &index,
                         AVFormatContext *fmt_ctx, int streamIndex)
        : fmt_ctx(fmt_ctx), streamIndex(streamIndex), packet(new_AVPacket()) {
        for (int f : frameIndicesRequested) {
            auto &pktIndex = index[f];
            auto &entry = schedule[pktIndex.keyFrameIndex];
            entry.keyFramePts = index[pktIndex.keyFrameIndex].pts;
            entry.neededPts.insert(pktIndex.pts);
            entry.lastPacketIndex =
                std::max(entry.lastPacketIndex, pktIndex.packetIndex);
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
    int requestIndex;
    int64_t pts;
};

void Video::getBatch(const std::vector<int> &frameIndices) {
    this->weakUp();

    std::vector<FrameRequest> request(frameIndices.size());
    for (int i = 0; i < frameIndices.size(); i++) {
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

    VideoPacketScheduler packetScheduler(frameIndices, packetIndex,
                                         this->fmt_ctx, this->streamIndex);

    auto decodeContext = new_AVCodecContext(decoder);

    CHECK_AV(avcodec_parameters_to_context(
                 decodeContext.get(), fmt_ctx->streams[streamIndex]->codecpar),
             "failed to set codec parameters");
    CHECK_AV(avcodec_open2(decodeContext.get(), decoder, nullptr),
             "open decoder failed");

    auto frame = new_AVFrame();

    bool eof = false;
    while (!eof) {
        while (!packetScheduler.finished()) {
            auto packet = packetScheduler.next();
            int ret = avcodec_send_packet(decodeContext.get(), packet);
            // std::cout << "Packet: " << packet->pts << " ret " << ret
            //           << std::endl;
            if (ret == AVERROR(EAGAIN)) {
                break;
            }
            CHECK_AV(ret, "send packet to decoder failed");
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

            // TODO: consume frame
            // std::cout << "Got frame: PTS: " << frame->pts << " flags: " <<
            // frame->flags << std::endl;
        }
    }
}

Video &Video::operator=(Video &&other) noexcept {
    if (this != &other) {
        dispose();
        this->fmt_ctx = other.fmt_ctx;
        other.fmt_ctx = nullptr;
        this->packetIndex = std::move(other.packetIndex);
        this->ioContext = std::move(other.ioContext);
        this->decoder = other.decoder;
        this->streamIndex = other.streamIndex;
    }
    return *this;
}

void Video::dispose() {
    if (this->fmt_ctx == nullptr) {
        return; // Moved.
    }
    this->sleep();
    avformat_close_input(&this->fmt_ctx);
}

void Video::sleep() {
    if (!isSleeping()) {
        this->getFileIO().sleep();
        av_freep(&this->ioContext->buffer); // to save memory
    }
}

FileIO &Video::getFileIO() {
    return *static_cast<FileIO *>(this->ioContext->opaque);
}

void Video::weakUp() {
    if (this->isSleeping()) {
        this->getFileIO().weakUp();
        ioContext->buffer = (uint8_t *)av_malloc(IO_BUFFER_SIZE);
        ioContext->buffer_size = ioContext->orig_buffer_size = IO_BUFFER_SIZE;
        ioContext->buf_ptr = ioContext->buf_end = ioContext->buf_ptr_max =
            ioContext->buffer;
    }
}

bool Video::isSleeping() { return this->getFileIO().isSleeping(); }

} // namespace videoloader
} // namespace huww
