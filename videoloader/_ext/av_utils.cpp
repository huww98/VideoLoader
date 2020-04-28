#include "av_utils.h"

#include <sstream>

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
