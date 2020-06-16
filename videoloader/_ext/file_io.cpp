#include "file_io.h"

#include <sstream>

namespace huww {
namespace videoloader {

void file_io::open_io() {
    fstream.open(file_path, std::fstream::in | std::fstream::binary);
    if (!fstream) {
        std::ostringstream msg;
        msg << "Unable to open file \"" << file_path << "\"";
        throw std::system_error(errno, std::system_category(), msg.str());
    }
}

file_io::file_io(std::string file_path) : file_path(file_path), last_pos(0) {
    this->open_io();
}

bool file_io::is_sleeping() { return !fstream.is_open(); }

void file_io::sleep() {
    if (!is_sleeping()) {
        last_pos = fstream.tellg();
        fstream.close();
    }
}

void file_io::wake_up() {
    if (is_sleeping()) {
        this->open_io();
        fstream.seekg(last_pos, std::fstream::beg);
    }
}

int file_io::read(uint8_t *buf, int size) {
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

int64_t file_io::seek(int64_t pos, int whence) {
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

avio_context_ptr file_io::new_avio_context(std::string file_path) {
    auto io = std::make_unique<file_io>(file_path);
    uint8_t *buffer = (uint8_t *)av_malloc(IO_BUFFER_SIZE);
    if (!buffer) {
        throw std::bad_alloc();
    }
    return avio_context_ptr(
        avio_alloc_context(
            buffer, IO_BUFFER_SIZE, 0, io.release(),
            [](void *opaque, uint8_t *buf, int buf_size) {
                return static_cast<file_io *>(opaque)->read(buf, buf_size);
            },
            nullptr,
            [](void *opaque, int64_t offset, int whence) {
                return static_cast<file_io *>(opaque)->seek(offset, whence);
            }),
        [](AVIOContext *c) {
            av_freep(&c->buffer);
            delete static_cast<file_io *>(c->opaque);
            avio_context_free(&c);
        });
}

} // namespace videoloader
} // namespace huww
