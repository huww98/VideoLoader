#include "file_io.h"

#include <sstream>

namespace huww {
namespace videoloader {

std::istream &file_io::current_stream() {
    if (this->external_stream != nullptr) {
        return *this->external_stream;
    }
    return fstream;
}

void file_io::set_external_stream(std::istream *stream) { this->external_stream = stream; }

void file_io::open_io() {
    fstream.open(file_path, std::fstream::in | std::fstream::binary);
    if (!fstream) {
        std::ostringstream msg;
        msg << "Unable to open file \"" << file_path << "\"";
        throw std::system_error(errno, std::system_category(), msg.str());
    }
}

file_io::file_io(std::string file_path, std::streampos start_pos, std::streamsize file_size,
                 std::istream *external_stream)
    : file_path(file_path), start_pos(start_pos), file_size(file_size),
      external_stream(external_stream) {

    if (external_stream == nullptr) {
        open_io();
    }
    if (file_size < 0) {
        auto &s = current_stream();
        this->file_size = s.seekg(0, std::istream::end).tellg() - start_pos;
        s.seekg(start_pos);
    }
}

bool file_io::is_sleeping() {
    return external_stream == nullptr && !fstream.is_open();
}

void file_io::sleep() {
    external_stream = nullptr;
    if (!is_sleeping()) {
        fstream.close();
    }
}

void file_io::wake_up() {
    if (is_sleeping()) {
        this->open_io();
        fstream.seekg(last_pos);
    }
}

int file_io::read(uint8_t *buf, int size) {
    size = std::min(std::streamsize(size), start_pos + file_size - last_pos);
    if (size <= 0) {
        return AVERROR_EOF;
    }

    auto &s = current_stream();
    s.read((char *)buf, size);
    if (s.eof()) {
        s.clear();
        if (s.gcount() == 0) {
            return AVERROR_EOF;
        }
    }
    if (!s) {
        return AVERROR(errno);
    }
    auto read_count = s.gcount();
    last_pos += read_count;
    return read_count;
}

int64_t file_io::seek(int64_t pos, int whence) {
    std::istream::seekdir dir;
    switch (whence) {
    case SEEK_SET:
        pos += this->start_pos;
        last_pos = pos;
        dir = std::istream::beg;
        break;
    case SEEK_CUR:
        last_pos += pos;
        dir = std::istream::cur;
        break;
    case SEEK_END:
        dir = std::istream::beg;
        last_pos = pos = pos + start_pos + file_size;
        break;
    case AVSEEK_SIZE:
        return file_size;
    default:
        return -1;
    }
    auto &s = current_stream();
    s.seekg(pos, dir);
    if (!s) {
        return AVERROR(errno);
    }
    return last_pos - start_pos;
}

avio_context_ptr file_io::new_avio_context(const file_spec &spec) {
    auto io =
        std::make_unique<file_io>(spec.path, spec.start_pos, spec.file_size, spec.external_stream);
    uint8_t *buffer = (uint8_t *)av_malloc(IO_BUFFER_SIZE);
    if (!buffer) {
        throw std::bad_alloc();
    }
    return avio_context_ptr(avio_alloc_context(
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
