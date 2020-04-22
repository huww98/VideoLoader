#pragma once

#include <stdexcept>
#include <string>

namespace huww {
namespace videoloader {

class AvError : public std::runtime_error {
  public:
    AvError(int errorCode, std::string message);
};

class VideoLoader {
  public:
    void addVideoFile(std::string file_path);
};
} // namespace videoloader
} // namespace huww
