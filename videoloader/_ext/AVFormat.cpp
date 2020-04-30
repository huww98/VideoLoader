#include "AVFormat.h"

#include "av_utils.h"

namespace huww {
namespace videoloader {

AVFormat::AVFormat(std::string url) : ioContext(FileIO::newAVIOContext(url)) {
    this->fmt_ctx =
        CHECK_AV(avformat_alloc_context(), "Unable to alloc AVFormatContext");

    // Use custom IO, manage AVIOContext ourself to save memory and other
    // resources.
    this->fmt_ctx->pb = ioContext.get();

    CHECK_AV(avformat_open_input(&this->fmt_ctx, url.c_str(), nullptr, nullptr),
             "Unable to open input \"" << url << "\"");
}


AVFormat &AVFormat::operator=(AVFormat &&other) noexcept {
    if (this != &other) {
        dispose();
        this->fmt_ctx = other.fmt_ctx;
        other.fmt_ctx = nullptr;
        this->ioContext = std::move(other.ioContext);
    }
    return *this;
}

void AVFormat::dispose() {
    if (this->fmt_ctx == nullptr) {
        return; // Moved.
    }
    avformat_close_input(&this->fmt_ctx);
}

FileIO &getFileIO(AVIOContextPtr &ctx) {
    return *static_cast<FileIO *>(ctx->opaque);
}

void AVFormat::sleep() {
    if (!isSleeping()) {
        getFileIO(this->ioContext).sleep();
        av_freep(&this->ioContext->buffer); // to save memory
    }
}

void AVFormat::weakUp() {
    if (this->isSleeping()) {
        getFileIO(this->ioContext).weakUp();
        ioContext->buffer = (uint8_t *)av_malloc(IO_BUFFER_SIZE);
        ioContext->buffer_size = ioContext->orig_buffer_size = IO_BUFFER_SIZE;
        ioContext->buf_ptr = ioContext->buf_end = ioContext->buf_ptr_max =
            ioContext->buffer;
    }
}

bool AVFormat::isSleeping() { return getFileIO(this->ioContext).isSleeping(); }

} // namespace videoloader
} // namespace huww
