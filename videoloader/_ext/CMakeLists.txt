cmake_minimum_required(VERSION 3.10)

project(videoloader CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)
set(VIDEOLOADER_NAME "" CACHE STRING "videoloader output filename, specified by setup.py")
set(VIDEOLOADER_DESTINATION "" CACHE PATH "videoloader output path, specified by setup.py")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall)

set(VIDEO_LOADER_SRCS videoloader.cpp av_utils.cpp AVFormat.cpp FileIO.cpp AVFilterGraph.cpp VideoDLPack.cpp)
if(VIDEOLOADER_NAME)
    list(APPEND VIDEO_LOADER_SRCS index.cpp PyRef.cpp)
    find_package(Python COMPONENTS Development REQUIRED)
else()
    message(WARNING "Use setup.py to build with python bindings.")
    add_compile_options("$<$<CONFIG:DEBUG>:-fomit-frame-pointer;-fsanitize=address>")
    add_link_options("$<$<CONFIG:DEBUG>:-fomit-frame-pointer;-fsanitize=address>")
endif()

find_package(AvCodec REQUIRED)
find_package(AvFormat REQUIRED)
find_package(AvFilter REQUIRED)
find_package(AvUtil REQUIRED)

add_library(videoloader SHARED "${VIDEO_LOADER_SRCS}")
target_link_libraries(videoloader FFmpeg::AvCodec FFmpeg::AvFormat FFmpeg::AvFilter FFmpeg::AvUtil)
target_compile_definitions(videoloader PRIVATE __STDC_CONSTANT_MACROS)

add_executable(test_main main.cpp)
target_link_libraries(test_main videoloader)

if(VIDEOLOADER_NAME)
    target_link_libraries(videoloader Python::Module)
    set_target_properties(videoloader PROPERTIES
        OUTPUT_NAME "${VIDEOLOADER_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${VIDEOLOADER_DESTINATION}"
        PREFIX ""
        SUFFIX "")

    target_link_libraries(test_main Python::Python)
endif()
