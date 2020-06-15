cmake_minimum_required(VERSION 3.15)

project(videoloader CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)
set(videoloader_NAME "" CACHE STRING "videoloader output filename, specified by setup.py")
set(videoloader_DESTINATION "" CACHE PATH "videoloader output path, specified by setup.py")
set(LOG_LEVEL "$<IF:$<CONFIG:Debug>,0,2>" CACHE STRING "compile time logging level")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
add_compile_options(-Wall)

set(VIDEO_LOADER_SRCS
    videoloader.cpp
    av_utils.cpp
    avformat.cpp
    file_io.cpp
    avfilter_graph.cpp
    video_dlpack.cpp
    video_dataset_loader.cpp)
if(videoloader_NAME AND videoloader_DESTINATION)
    list(APPEND VIDEO_LOADER_SRCS
        index.cpp
        pyref.cpp)
    if (Python_VERSION)
        find_package(Python ${Python_VERSION} EXACT COMPONENTS Development NumPy REQUIRED)
    else()
        find_package(Python COMPONENTS Development NumPy REQUIRED)
    endif()
else()
    message(WARNING "Use setup.py to build with python bindings.")
    add_compile_options("$<$<CONFIG:DEBUG>:-fomit-frame-pointer;-fsanitize=address>")
    add_link_options("$<$<CONFIG:DEBUG>:-fomit-frame-pointer;-fsanitize=address>")
endif()

find_package(AvCodec REQUIRED)
find_package(AvFormat REQUIRED)
find_package(AvFilter REQUIRED)
find_package(AvUtil REQUIRED)

add_subdirectory(third_party/spdlog)
set_property(TARGET spdlog PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(videoloader SHARED "${VIDEO_LOADER_SRCS}")
target_link_libraries(videoloader FFmpeg::AvCodec FFmpeg::AvFormat FFmpeg::AvFilter FFmpeg::AvUtil spdlog)
target_compile_definitions(videoloader PRIVATE __STDC_CONSTANT_MACROS)
target_compile_definitions(videoloader PRIVATE SPDLOG_ACTIVE_LEVEL=${LOG_LEVEL})

add_executable(test_main main.cpp)
target_link_libraries(test_main videoloader spdlog)

if(videoloader_NAME)
    target_link_libraries(videoloader Python::Module Python::NumPy)
    target_compile_definitions(videoloader PRIVATE NPY_NO_DEPRECATED_API=NPY_1_7_API_VERSION)
    set_target_properties(videoloader PROPERTIES
        OUTPUT_NAME "${videoloader_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${videoloader_DESTINATION}"
        PREFIX ""
        SUFFIX "")

    target_link_libraries(test_main Python::Python)
endif()
