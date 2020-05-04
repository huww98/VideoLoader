#pragma once

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

namespace huww {
namespace videoloader {

class AvError : public std::runtime_error {
  private:
    int _code;

  public:
    AvError(int errorCode, std::string message);
    int code() const noexcept { return this->_code; }
};

inline int check_av(int retCode) { return retCode; }
inline int check_av(void *ptr) { return ptr == nullptr ? AVERROR(ENOMEM) : 0; }
#define CHECK_AV(call, msg)                                                    \
    [&] {                                                                      \
        auto __ret = (call);                                                   \
        int errCode = check_av(__ret);                                         \
        if (errCode < 0) {                                                     \
            std::ostringstream msgStream;                                      \
            msgStream << "at:" << __FILE__ << ':' << __LINE__ << '\n' << msg;  \
            throw AvError(errCode, msgStream.str());                           \
        }                                                                      \
        return __ret;                                                          \
    }()

using AVFramePtr = std::unique_ptr<AVFrame, void (*)(AVFrame *&&)>;

inline auto allocAVFrame() {
    return AVFramePtr(CHECK_AV(av_frame_alloc(), "alloc AVFrame failed"),
                      [](AVFrame *&&f) { av_frame_free(&f); });
}

} // namespace videoloader
} // namespace huww
