cmake_minimum_required(VERSION 3.10)

project(videoloader CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)
set(VIDEOLOADER_NAME "" CACHE STRING "videoloader output filename, specified by setup.py")
set(VIDEOLOADER_DESTINATION "" CACHE PATH "videoloader output path, specified by setup.py")

set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall)

find_package(Python COMPONENTS Development REQUIRED)
find_package(AvCodec REQUIRED)
find_package(AvFormat REQUIRED)
find_package(AvFilter REQUIRED)

add_library(videoloader SHARED PyRef.cpp videoloader.cpp index.cpp)
target_link_libraries(videoloader PRIVATE Python::Module FFmpeg::AvCodec FFmpeg::AvFormat FFmpeg::AvFilter)

if(VIDEOLOADER_NAME)
    set_target_properties(videoloader PROPERTIES
        OUTPUT_NAME "${VIDEOLOADER_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${VIDEOLOADER_DESTINATION}"
        PREFIX ""
        SUFFIX "")
else(VIDEOLOADER_NAME)
    message(WARNING "Use setup.py to put binaries to correct location")
endif(VIDEOLOADER_NAME)

add_executable(test_main main.cpp)
target_link_libraries(test_main Python::Python videoloader)
