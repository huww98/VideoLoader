#pragma once

#include <string>
#include <stdexcept>

extern "C" {
    #include <libavutil/error.h>
}

class AvError : public std::runtime_error {
  public:
    AvError(int errorCode, std::string message);
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
