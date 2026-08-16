cmake_minimum_required(VERSION 3.20)
project(empf-thumbs LANGUAGES C CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_LIBWEBPMUX OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libwebp
    URL https://github.com/webmproject/libwebp/archive/refs/tags/v1.4.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(libwebp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX MINIZ_NO_STDIO)

set(MINIZ_SOURCES
    vendor/miniz/miniz.c
    vendor/miniz/miniz_tdef.c
    vendor/miniz/miniz_tinfl.c
    vendor/miniz/miniz_zip.c
)

set(EMPF_COMMON_SOURCES
    src/EmpfReader.cpp
    src/WicBitmap.cpp
    ${MINIZ_SOURCES}
)

add_library(EmpfThumbs SHARED
    ${EMPF_COMMON_SOURCES}
    src/ClassFactory.cpp
    src/dllmain.cpp
    src/PreviewHandler.cpp
    src/Registry.cpp
    src/ThumbnailProvider.cpp
    EmpfThumbs.def
)

target_include_directories(EmpfThumbs PRIVATE src vendor/miniz ${libwebp_SOURCE_DIR}/src)
target_link_libraries(EmpfThumbs PRIVATE
    bcrypt
    windowscodecs
    shlwapi
    ole32
    uuid
    gdi32
    user32
    advapi32
    propsys
    shell32
    crypt32
    webpdecoder
)
set_target_properties(EmpfThumbs PROPERTIES
    OUTPUT_NAME EmpfThumbs
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

add_executable(empf-extract
    src/extract_main.cpp
    src/EmpfReader.cpp
    src/WicBitmap.cpp
    ${MINIZ_SOURCES}
)
target_include_directories(empf-extract PRIVATE src vendor/miniz ${libwebp_SOURCE_DIR}/src)
target_link_libraries(empf-extract PRIVATE bcrypt windowscodecs shlwapi ole32 gdi32 uuid crypt32 webpdecoder)
set_target_properties(empf-extract PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

set(PAYLOAD_RC "${CMAKE_CURRENT_BINARY_DIR}/payload.rc")
add_custom_command(
    OUTPUT "${PAYLOAD_RC}"
    COMMAND ${CMAKE_COMMAND}
        -DDLL=$<TARGET_FILE:EmpfThumbs>
        -DEXTRACT=$<TARGET_FILE:empf-extract>
        -DOUT=${PAYLOAD_RC}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_payload_rc.cmake"
    DEPENDS EmpfThumbs empf-extract
    VERBATIM
)

add_executable(EmpfThumbsSetup WIN32
    src/setup_main.cpp
    "${PAYLOAD_RC}"
)
target_link_libraries(EmpfThumbsSetup PRIVATE ole32 shell32 shlwapi advapi32 user32)
set_target_properties(EmpfThumbsSetup PROPERTIES
    OUTPUT_NAME EmpfThumbsSetup
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
add_dependencies(EmpfThumbsSetup EmpfThumbs empf-extract)
