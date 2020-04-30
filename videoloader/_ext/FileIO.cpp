#include "FileIO.h"

#include <sstream>

namespace huww {
namespace videoloader {

void FileIO::openIO() {
    fstream.open(filePath, std::fstream::in | std::fstream::binary);
    if (!fstream) {
        std::ostringstream msg;
        msg << "Unable to open file \"" << filePath << "\"";
        throw std::system_error(errno, std::system_category(), msg.str());
    }
}

FileIO::FileIO(std::string filePath) : filePath(filePath), lastPos(0) {
    this->openIO();
}

bool FileIO::isSleeping() { return !fstream.is_open(); }

void FileIO::sleep() {
    if (!isSleeping()) {
        lastPos = fstream.tellg();
        fstream.close();
    }
}

void FileIO::weakUp() {
    if (isSleeping()) {
        this->openIO();
        fstream.seekg(lastPos, std::fstream::beg);
    }
}

int FileIO::read(uint8_t *buf, int size) {
    fstream.read((char *)buf, size);
    if (fstream.eof()) {
        fstream.clear();
        if (fstream.gcount() == 0) {
            return AVERROR_EOF;
        }
    }
    if(!fstream) {
        return AVERROR(errno);
    }
    return fstream.gcount();
}

int64_t FileIO::seek(int64_t pos, int whence) {
    std::fstream::seekdir dir;
    switch (whence) {
    case SEEK_SET:
        dir = std::fstream::beg;
        break;
    case SEEK_CUR:
        dir = std::fstream::cur;
        break;
    case SEEK_END:
        dir = std::fstream::end;
        break;
    default:
        return -1;
    }
    fstream.seekg(pos, dir);
    if (!fstream) {
        return AVERROR(errno);
    }
    return fstream.tellg();
}

AVIOContextPtr FileIO::newAVIOContext(std::string filePath) {
    uint8_t *buffer = (uint8_t *)av_malloc(IO_BUFFER_SIZE);
    auto io = new FileIO(filePath);
    return AVIOContextPtr(
        avio_alloc_context(
            buffer, IO_BUFFER_SIZE, 0, io,
            [](void *opaque, uint8_t *buf, int buf_size) {
                return static_cast<FileIO *>(opaque)->read(buf, buf_size);
            },
            nullptr,
            [](void *opaque, int64_t offset, int whence) {
                return static_cast<FileIO *>(opaque)->seek(offset, whence);
            }),
        [](AVIOContext *&&c) {
            av_freep(&c->buffer);
            delete static_cast<FileIO *>(c->opaque);
            avio_context_free(&c);
        });
}

} // namespace videoloader
} // namespace huww
