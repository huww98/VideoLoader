#pragma once

#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "AVFilterGraph.h"
#include "AVFormat.h"
#include "VideoDLPack.h"

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
    /** Sorted by pts */
    std::vector<packet_index_entry> packet_index;

    AVStream &current_stream() noexcept;

  public:
    explicit video(std::string url);

    void sleep();
    void wake_up();
    bool is_sleeping();

    size_t num_frames() const noexcept { return packet_index.size(); }
    AVRational average_frame_rate() noexcept;

    video_dlpack::ptr get_batch(const std::vector<std::size_t> &frame_indices,
                              dlpack_pool *pool = nullptr);
};

class video_loader {
  public:
    video add_video_file(std::string url);
};

} // namespace videoloader
} // namespace huww
