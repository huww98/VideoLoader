#include "videoloader.h"
#include <iostream>
#include <sstream>

namespace huww {
namespace videoloader {

std::string getMessage(int errorCode, std::string message) {
    char errstr[256];
    av_strerror(errorCode, errstr, 256);
    if (message.empty())
        return errstr;

    std::ostringstream msgStream;
    msgStream << message << ": " << errstr;
    return msgStream.str();
}
AvError::AvError(int errorCode, std::string message)
    : std::runtime_error(getMessage(errorCode, message)) {}

int check_av(int retCode) { return retCode; }
int check_av(void *ptr) { return ptr == nullptr ? AVERROR(ENOMEM) : 0; }
#define CHECK_AV(call, msg)                                                    \
    [&] {                                                                      \
        auto ret = (call);                                                     \
        int errCode = check_av(ret);                                           \
        if (errCode < 0) {                                                     \
            std::ostringstream msgStream;                                      \
            msgStream << "at:" << __FILE__ << ':' << __LINE__ << '\n' << msg;  \
            throw AvError(errCode, msgStream.str());                           \
        }                                                                      \
        return ret;                                                            \
    }()

Video VideoLoader::addVideoFile(std::string url) {
    Video v(url);
    v.sleep();
    return v;
}

Video::Video(std::string url) : url(url), inSync(true) {
    this->fmt_ctx = avformat_alloc_context();
    CHECK_AV(fmt_ctx, "Unable to alloc AVFormatContext");

    // Use custom IO, manage AVIOContext ourself to save memory and other
    // resources.
    this->openIO();

    CHECK_AV(avformat_open_input(&this->fmt_ctx, url.c_str(), nullptr, nullptr),
             "Unable to open input \"" << url << "\"");
}

Video &Video::operator=(Video &&other) {
    if (this != &other) {
        dispose();
        this->fmt_ctx = other.fmt_ctx;
        other.fmt_ctx = nullptr;
        this->url = std::move(other.url);
        this->inSync = other.inSync;
    }
    return *this;
}

void Video::dispose() {
    if (this->fmt_ctx == nullptr) {
        return; // Moved.
    }
    this->sleep();
    avformat_close_input(&this->fmt_ctx);
}

void Video::openIO() {
    CHECK_AV(
        avio_open2(&fmt_ctx->pb, url.c_str(), AVIO_FLAG_READ, nullptr, nullptr),
        "Unable to open IO for input \"" << url << "\"");
}

void Video::sleep() {
    this->inSync = false;
    if (!sleeping()) {
        CHECK_AV(avio_closep(&fmt_ctx->pb), "Failed to close io");
    }
}

void Video::weakUp() {
    if (this->sleeping()) {
        this->openIO();
    }
}

bool Video::sleeping() { return fmt_ctx->pb == nullptr; }

} // namespace videoloader
} // namespace huww
