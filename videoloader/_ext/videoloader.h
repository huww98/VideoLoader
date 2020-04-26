#pragma once

#include <stdexcept>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace huww {
namespace videoloader {

class AvError : public std::runtime_error {
  public:
    AvError(int errorCode, std::string message);
};

class Video {
  private:
    std::string url;
    bool inSync = false;
    AVFormatContext *fmt_ctx = nullptr;
    void openIO();
    void dispose();

  public:
    Video(std::string url);
    Video(Video &&other) { *this = std::move(other); }
    Video &operator=(Video &&other);
    Video(const Video &) = delete;
    Video &operator=(const Video &) = delete;
    ~Video() { this->dispose(); }

    void sleep();
    void weakUp();
    bool sleeping();
};

class VideoLoader {
  public:
    Video addVideoFile(std::string url);
};

} // namespace videoloader
} // namespace huww
