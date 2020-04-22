#include "videoloader.h"
#include <sstream>

extern "C" {
#include <libavformat/avformat.h>
}

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
AvError::AvError(int errorCode, std::string message) : std::runtime_error(getMessage(errorCode, message)) {}

void VideoLoader::addVideoFile(std::string file_path) {
    AVFormatContext *fmt_ctx = nullptr;
    int open_ret = avformat_open_input(&fmt_ctx, file_path.c_str(), NULL, NULL);
    if (open_ret != 0) {
        std::ostringstream msg;
        msg << "Unable to open input file \"" << file_path << "\"";
        throw AvError(open_ret, msg.str());
    }
}
} // namespace videoloader
} // namespace huww
