cmake_minimum_required(VERSION 3.14)

project(car VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "PLATFORM=x64")
    set(PLATFORM x64)
else()
    message(STATUS "PLATFORM=x86")
    set(PLATFORM x86)
endif()

if (NOT CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

find_package(Qt6 COMPONENTS Core WebSockets Network REQUIRED)

include_directories(
    ../common
    ../base
)

link_directories(
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

aux_source_directory(. SELF_SRCS)

add_executable(car 
    ${SELF_SRCS}
)

target_link_libraries(car Qt6::Core Qt6::WebSockets Qt6::Network common base)

if(WIN32)
    add_custom_command(
            TARGET car POST_BUILD
            COMMAND ${QT_LOCATION}/bin/windeployqt
            --core --network --websockets --test
            $<TARGET_FILE:car> WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

add_custom_command(
    TARGET car POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/ConfigCar.ini
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ConfigCar.ini)

add_custom_command(
    TARGET car POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/VERSION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/VERSION)

if(UNIX)
    add_custom_command(
        TARGET car POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/../third_party/ffmpeg/${OS}/${PLATFORM}/bin/ffmpeg
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ffmpeg)
endif()

