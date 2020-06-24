#pragma once

#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "avfilter_graph.h"
#include "avformat.h"
#include "video_dlpack.h"

namespace huww {
namespace videoloader {

void init();

struct packet_index_entry {
    int64_t pts;
    int key_frame_index;
    int packet_index;
};

class video {
  private:
    avformat format;
    AVCodec *decoder = nullptr;
    int stream_index = -1;
    /**
     * Index for every frame sorted by PTS. Used to convert frame index to PTS.
     *
     * \note We don't use the index from `AVStream::index_entries`, because:
     * - It's not guaranteed to be presented.
     * - Whether the timestamp is PTS or DTS is not defined, it is internal to demuxer. mp4 use DTS
     */
    std::vector<packet_index_entry> packet_index;

    AVStream &current_stream() noexcept;

  public:
    explicit video(std::string url);
    video(const file_io::file_spec &spec);

    /**
     * Indicate this video will not be read recently. Discard all buffer to save memory. Close IO
     * interface to save file descriptors.
     */
    void sleep();
    void wake_up();
    bool is_sleeping();

    size_t num_frames() const noexcept { return packet_index.size(); }
    AVRational average_frame_rate() noexcept;

    video_dlpack::ptr get_batch(const std::vector<std::size_t> &frame_indices,
                                dlpack_pool *pool = nullptr);
};

} // namespace videoloader
} // namespace huww
