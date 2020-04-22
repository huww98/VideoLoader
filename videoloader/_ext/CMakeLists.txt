cmake_minimum_required(VERSION 3.10)

project(videoloader CXX)

set(VIDEOLOADER_NAME "" CACHE STRING "videoloader output filename, specified by setup.py")
set(VIDEOLOADER_DESTINATION "" CACHE PATH "videoloader output path, specified by setup.py")

set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall)
add_compile_options("$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer;-fsanitize=address>")
add_link_options("$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer;-fsanitize=address>")

find_package(Python COMPONENTS Development REQUIRED)
add_library(videoloader SHARED index.cpp)
target_link_libraries(videoloader Python::Module)

if(VIDEOLOADER_NAME)
    set_target_properties(videoloader PROPERTIES
        OUTPUT_NAME "${VIDEOLOADER_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${VIDEOLOADER_DESTINATION}"
        PREFIX ""
        SUFFIX "")
else(VIDEOLOADER_NAME)
    message(WARNING "Use setup.py to put binaries to correct location")
endif(VIDEOLOADER_NAME)
