#pragma once

#include <memory>
#include <string>
#include <fstream>

extern "C" {
#include <libavformat/avio.h>
}

namespace huww {
namespace videoloader {

using avio_context_ptr = std::unique_ptr<AVIOContext, void (*)(AVIOContext *c)>;
constexpr int IO_BUFFER_SIZE = 32768;

class file_io {
  private:
    std::string file_path;
    std::ifstream fstream;
    std::streampos last_pos;

    void open_io();

  public:
    file_io(std::string file_path);

    bool is_sleeping();
    void sleep();
    void wake_up();

    int read(uint8_t *buf, int size);
    int64_t seek(int64_t pos, int whence);

    static avio_context_ptr new_avio_context(std::string file_path);
};

} // namespace videoloader
} // namespace huww
