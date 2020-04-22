cmake_minimum_required(VERSION 3.10)

project(videoloader CXX)

set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall)
add_compile_options("$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer;-fsanitize=address>")
add_link_options("$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer;-fsanitize=address>")

find_package(Python COMPONENTS Development REQUIRED)
add_library(videoloader SHARED index.cpp)
target_link_libraries(videoloader Python::Module)
set_target_properties(videoloader PROPERTIES OUTPUT_NAME "${VIDEOLOADER_NAME}"
                                             LIBRARY_OUTPUT_DIRECTORY "${VIDEOLOADER_DESTINATION}"
                                             PREFIX ""
                                             SUFFIX "")
