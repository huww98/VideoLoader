#include "av_utils.h"

#include <sstream>

namespace huww {
namespace videoloader {

std::string get_message(int error_code, std::string message) {
    char errstr[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(error_code, errstr, sizeof(errstr));
    if (message.empty())
        return errstr;

    std::ostringstream msg_stream;
    msg_stream << message << ": " << errstr;
    return msg_stream.str();
}
av_error::av_error(int error_code, std::string message)
    : std::runtime_error(get_message(error_code, message)), _code(error_code) {}

} // namespace videoloader
} // namespace huww
