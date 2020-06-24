#pragma once

#include <fstream>
#include <memory>
#include <string>

extern "C" {
#include <libavformat/avio.h>
}

namespace huww {
namespace videoloader {

struct avio_context_deleter {
    void operator()(AVIOContext *);
};
using avio_context_ptr = std::unique_ptr<AVIOContext, avio_context_deleter>;
constexpr int IO_BUFFER_SIZE = 32768;

class file_io {
  private:
    std::string file_path;
    std::ifstream fstream;
    std::streampos last_pos = 0;
    std::streampos start_pos;
    std::streamsize file_size;
    std::istream *external_stream = nullptr;

    void open_io();
    std::istream &current_stream();

  public:
    file_io(std::string file_path, std::streampos start_pos = 0, std::streamsize file_size = -1,
            std::istream *external_stream = nullptr);

    void set_external_stream(std::istream *stream);

    bool is_sleeping();
    void sleep();
    void wake_up();

    int read(uint8_t *buf, int size);
    int64_t seek(int64_t pos, int whence);

    struct file_spec {
        std::string path;
        std::streampos start_pos = 0;
        std::streamsize file_size = -1;
        std::istream *external_stream = nullptr;
    };
    static avio_context_ptr new_avio_context(const file_spec &spec);
};

} // namespace videoloader
} // namespace huww
