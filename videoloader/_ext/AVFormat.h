#pragma once

extern "C" {
#include <libavformat/avformat.h>
}
#include "FileIO.h"

namespace huww {
namespace videoloader {

class AVFormat {
  private:
    AVIOContextPtr ioContext;
    AVFormatContext *fmt_ctx = nullptr;
    void dispose();

  public:
    AVFormat(std::string url);
    AVFormat(AVFormat &&other) noexcept : ioContext(nullptr, nullptr) {
        *this = std::move(other);
    }
    AVFormat &operator=(AVFormat &&other) noexcept;
    AVFormat(const AVFormat &) = delete;
    AVFormat &operator=(const AVFormat &) = delete;
    ~AVFormat() { this->dispose(); }

    void sleep();
    void weakUp();
    bool isSleeping();
    AVFormatContext *formatContext() { return this->fmt_ctx; }
};

} // namespace videoloader
} // namespace huww
