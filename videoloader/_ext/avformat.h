#pragma once

extern "C" {
#include <libavformat/avformat.h>
}
#include "file_io.h"

namespace huww {
namespace videoloader {

class avformat {
  private:
    avio_context_ptr io_context;
    AVFormatContext *fmt_ctx = nullptr;
    void dispose();

  public:
    explicit avformat(std::string url);
    avformat(const file_io::file_spec &spec);
    avformat(avformat &&other) noexcept {
        *this = std::move(other);
    }
    avformat &operator=(avformat &&other) noexcept;
    avformat(const avformat &) = delete;
    avformat &operator=(const avformat &) = delete;
    ~avformat() { this->dispose(); }

    void sleep();
    void wake_up();
    bool is_sleeping();
    AVFormatContext *format_context() { return this->fmt_ctx; }
};

} // namespace videoloader
} // namespace huww
