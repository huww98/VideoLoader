#pragma once

#include <memory>
#include <string>
#include <fstream>

extern "C" {
#include <libavformat/avio.h>
}

namespace huww {
namespace videoloader {

using AVIOContextPtr = std::unique_ptr<AVIOContext, void (*)(AVIOContext *&&c)>;
constexpr int IO_BUFFER_SIZE = 32768;

class FileIO {
  private:
    std::string filePath;
    std::ifstream fstream;
    std::streampos lastPos;

    void openIO();

  public:
    FileIO(std::string filePath);

    bool isSleeping();
    void sleep();
    void weakUp();

    int read(uint8_t *buf, int size);
    int64_t seek(int64_t pos, int whence);

    static AVIOContextPtr newAVIOContext(std::string filePath);
};

} // namespace videoloader
} // namespace huww
