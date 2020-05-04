#include "av_utils.h"

#include <sstream>

namespace huww {
namespace videoloader {

std::string getMessage(int errorCode, std::string message) {
    char errstr[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errorCode, errstr, sizeof(errstr));
    if (message.empty())
        return errstr;

    std::ostringstream msgStream;
    msgStream << message << ": " << errstr;
    return msgStream.str();
}
AvError::AvError(int errorCode, std::string message)
    : std::runtime_error(getMessage(errorCode, message)), _code(errorCode) {}

} // namespace videoloader
} // namespace huww
