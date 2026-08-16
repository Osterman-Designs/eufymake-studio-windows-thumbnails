cmake_minimum_required(VERSION 3.20)
project(empf-thumbs LANGUAGES C CXX)

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

target_include_directories(EmpfThumbs PRIVATE src vendor/miniz)
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
)
set_target_properties(EmpfThumbs PROPERTIES
    OUTPUT_NAME EmpfThumbs
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

add_executable(empf-extract
    src/extract_main.cpp
    src/EmpfReader.cpp
    ${MINIZ_SOURCES}
)
target_include_directories(empf-extract PRIVATE src vendor/miniz)
target_link_libraries(empf-extract PRIVATE bcrypt)
set_target_properties(empf-extract PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
