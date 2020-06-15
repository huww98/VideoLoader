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

class av_error : public std::runtime_error {
  private:
    int _code;

  public:
    av_error(int error_code, std::string message);
    int code() const noexcept { return this->_code; }
};

inline int check_av(int ret_code) { return ret_code; }
inline int check_av(void *ptr) { return ptr == nullptr ? AVERROR(ENOMEM) : 0; }
#define CHECK_AV(call, msg)                                                                        \
    [&] {                                                                                          \
        auto __ret = (call);                                                                       \
        int __err_code = check_av(__ret);                                                          \
        if (__err_code < 0) {                                                                      \
            std::ostringstream __msg_stream;                                                       \
            __msg_stream << "at:" << __FILE__ << ':' << __LINE__ << '\n' << msg;                   \
            throw av_error(__err_code, __msg_stream.str());                                        \
        }                                                                                          \
        return __ret;                                                                              \
    }()

struct avframe_deleter {
    void operator()(AVFrame *f) { av_frame_free(&f); }
};

using avframe_ptr = std::unique_ptr<AVFrame, avframe_deleter>;

inline auto new_avframe() {
    return avframe_ptr(CHECK_AV(av_frame_alloc(), "alloc AVFrame failed"));
}

} // namespace videoloader
} // namespace huww
