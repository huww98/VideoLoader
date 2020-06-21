#include "avformat.h"

#include "av_utils.h"

namespace huww {
namespace videoloader {

avformat::avformat(std::string url) : avformat(file_io::file_spec{.path = url}) {}

avformat::avformat(const file_io::file_spec &spec) : io_context(file_io::new_avio_context(spec)) {
    this->fmt_ctx = CHECK_AV(avformat_alloc_context(), "Unable to alloc AVFormatContext");

    // Use custom IO, manage AVIOContext ourself to save memory and other resources.
    this->fmt_ctx->pb = io_context.get();

    CHECK_AV(avformat_open_input(&this->fmt_ctx, spec.path.c_str(), nullptr, nullptr),
             "Unable to open input \"" << spec.path << "\"");
}

avformat &avformat::operator=(avformat &&other) noexcept {
    if (this != &other) {
        dispose();
        this->fmt_ctx = other.fmt_ctx;
        other.fmt_ctx = nullptr;
        this->io_context = std::move(other.io_context);
    }
    return *this;
}

void avformat::dispose() {
    if (this->fmt_ctx == nullptr) {
        return; // Moved.
    }
    avformat_close_input(&this->fmt_ctx);
}

file_io &get_file_io(avio_context_ptr &ctx) { return *static_cast<file_io *>(ctx->opaque); }

void avformat::sleep() {
    if (!is_sleeping()) {
        get_file_io(this->io_context).sleep();
        av_freep(&this->io_context->buffer); // to save memory
    }
}

void avformat::wake_up() {
    if (this->is_sleeping()) {
        get_file_io(this->io_context).wake_up();
        io_context->buffer = (uint8_t *)av_malloc(IO_BUFFER_SIZE);
        io_context->buffer_size = io_context->orig_buffer_size = IO_BUFFER_SIZE;
        io_context->buf_ptr = io_context->buf_end = io_context->buf_ptr_max = io_context->buffer;
    }
}

bool avformat::is_sleeping() { return get_file_io(this->io_context).is_sleeping(); }

} // namespace videoloader
} // namespace huww
