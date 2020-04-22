find_package(PkgConfig)
pkg_check_modules(PC_FFmpeg_avcodec QUIET libavcodec)
pkg_check_modules(PC_FFmpeg_avformat QUIET libavformat)
pkg_check_modules(PC_FFmpeg_avutil QUIET libavutil)

set(INCLUDE_PATHS /usr/include /usr/local/include /opt/local/include /sw/include)
set(LIBRARY_PATHS /usr/lib /usr/local/lib /opt/local/lib /sw/lib)

find_path(FFmpeg_avcodec_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    PATHS ${PC_FFmpeg_avcodec_INCLUDE_DIRS} ${INCLUDE_PATHS}
    PATH_SUFFIXES ffmpeg libav
)

find_library(FFmpeg_avcodec_LIBRARY
    NAMES avcodec
    PATHS ${PC_FFmpeg_avcodec_LIBRARY_DIRS} ${LIBRARY_PATHS}
)

find_library(FFmpeg_avformat_LIBRARY
    NAMES avformat
    PATHS ${PC_FFmpeg_avformat_LIBRARY_DIRS} ${LIBRARY_PATHS}
)

find_library(FFmpeg_avutil_LIBRARY
    NAMES avutil
    PATHS ${PC_FFmpeg_avutil_LIBRARY_DIRS} ${LIBRARY_PATHS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
    FFmpeg_avcodec_INCLUDE_DIR FFmpeg_avcodec_LIBRARY FFmpeg_avformat_LIBRARY
)

if(FFmpeg_FOUND)
    set(FFmpeg_INCLUDE_DIRS ${FFmpeg_avcodec_INCLUDE_DIR})
    set(FFmpeg_LIBRARIES
        ${FFmpeg_avcodec_LIBRARY}
        ${FFmpeg_avformat_LIBRARY}
        ${FFmpeg_avutil_LIBRARY}
    )

    if(FFmpeg_avcodec_LIBRARY AND NOT TARGET FFmpeg::avcodec)
        add_library(FFmpeg::avcodec UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::avcodec PROPERTIES
            IMPORTED_LOCATION "${FFmpeg_avcodec_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_FFmpeg_avcodec_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_avcodec_INCLUDE_DIR}"
        )
    endif()

    if(FFmpeg_avformat_LIBRARY AND NOT TARGET FFmpeg::avformat)
        add_library(FFmpeg::avformat UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::avformat PROPERTIES
            IMPORTED_LOCATION "${FFmpeg_avformat_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_FFmpeg_avformat_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_avformat_INCLUDE_DIR}"
        )
    endif()

    if(FFmpeg_avutil_LIBRARY AND NOT TARGET FFmpeg::avutil)
        add_library(FFmpeg::avutil UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::avutil PROPERTIES
            IMPORTED_LOCATION "${FFmpeg_avutil_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_FFmpeg_avutil_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_avutil_INCLUDE_DIR}"
        )
    endif()

    if(NOT TARGET FFmpeg::All)
        add_library(FFmpeg::All UNKNOWN IMPORTED)
        target_link_libraries(FFmpeg::All INTERFACE FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil)
    endif()
endif()
