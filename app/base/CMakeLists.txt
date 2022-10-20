cmake_minimum_required(VERSION 3.5)

#该目录封装一些常用功能函数，考虑跨平台，勿依赖QT
#linux下安装x264、x265
#   sudo apt install libx265-dev
#   sudo apt install libx264-dev

project(base LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SELF_SRCS 
    ./MediaTools.cpp
    ./BaseFunc.cpp
    ./SoLogger.cpp
    ./Defer.cpp
)

if(UNIX)
    set(SELF_SRCS
        ${SELF_SRCS}
        ./SysInfo.cpp
    )
endif()

add_library(base STATIC 
    ${SELF_SRCS}
)

target_link_directories(base BEFORE PUBLIC
    ${CMAKE_SOURCE_DIR}/../third_party/ffmpeg/${OS}/${PLATFORM}/lib
)

if(WIN32)
    target_compile_options(base PUBLIC /wd4819 /wd4996)
    file(GLOB FFmpegDlls ${CMAKE_SOURCE_DIR}/../third_party/ffmpeg/${OS}/${PLATFORM}/lib/*.dll) 
else()
    target_compile_options(base PRIVATE -Wno-unused-result)
    file(GLOB FFmpegDlls ${CMAKE_SOURCE_DIR}/../third_party/ffmpeg/linux/${PLATFORM}/lib/*.so.*)
endif()

foreach(DllFile ${FFmpegDlls})
add_custom_command(TARGET base POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DllFile} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endforeach()

target_include_directories(base PUBLIC
    ../../third_party/ffmpeg/${OS}/${PLATFORM}/include
)

target_link_libraries(base PUBLIC
    avformat
    avdevice
    avfilter
    avcodec
    postproc
    swresample
    swscale
    avutil
)

#if(UNIX)
#    target_link_libraries(base PUBLIC
#        z
#        x264
#        x265
#        asound
#        xcb
#        lzma
#    )
#endif()
