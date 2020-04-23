string(TOLOWER "${_FFmpeg_COMPONENT}" _LOWER_NAME)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_FFmpeg_${_FFmpeg_COMPONENT} QUIET "lib${_LOWER_NAME}")
    if(PC_FFmpeg_${_FFmpeg_COMPONENT}_FOUND)
        set(FFmpeg_${_FFmpeg_COMPONENT}_VERSION_STRING ${PC_FFmpeg_${_FFmpeg_COMPONENT}_VERSION})
    endif()
endif()

find_path(FFmpeg_${_FFmpeg_COMPONENT}_INCLUDE_DIR
    NAMES lib${_LOWER_NAME}/${_LOWER_NAME}.h
    HINTS ${PC_FFmpeg_${_FFmpeg_COMPONENT}_INCLUDE_DIRS}
)

find_library(FFmpeg_${_FFmpeg_COMPONENT}_LIBRARY
    NAMES ${_LOWER_NAME}
    HINTS ${PC_FFmpeg_${_FFmpeg_COMPONENT}_LIBRARY_DIRS}
)

mark_as_advanced(FFmpeg_${_FFmpeg_COMPONENT}_INCLUDE_DIR FFmpeg_${_FFmpeg_COMPONENT}_LIBRARY)

if(FFmpeg_${_FFmpeg_COMPONENT}_INCLUDE_DIR AND FFmpeg_${_FFmpeg_COMPONENT}_LIBRARY)
    if(NOT TARGET FFmpeg::${_FFmpeg_COMPONENT})
        add_library(FFmpeg::${_FFmpeg_COMPONENT} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${_FFmpeg_COMPONENT} PROPERTIES
            IMPORTED_LOCATION "${FFmpeg_${_FFmpeg_COMPONENT}_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_FFmpeg_${_FFmpeg_COMPONENT}_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_${_FFmpeg_COMPONENT}_INCLUDE_DIR}"
        )
    endif()
endif()


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args("${_FFmpeg_COMPONENT}"
    REQUIRED_VARS FFmpeg_${_FFmpeg_COMPONENT}_LIBRARY FFmpeg_${_FFmpeg_COMPONENT}_INCLUDE_DIR
    VERSION_VAR FFmpeg_${_FFmpeg_COMPONENT}_VERSION_STRING
)
