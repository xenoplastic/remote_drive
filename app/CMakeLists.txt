cmake_minimum_required(VERSION 3.14)

project (app)
if (NOT DEFINED QT_LOCATION)
    message(FATAL_ERROR "QT_LOCATION must be defined.")
endif ()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "PLATFORM=x64")
    set(PLATFORM x64)
else()
    message(STATUS "PLATFORM=x86")
    set(PLATFORM x86)
endif()

if (NOT CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

message(STATUS "QT_LOCATION=" ${QT_LOCATION})
message(STATUS "CMAKE_BUILD_TYPE=" ${CMAKE_BUILD_TYPE})

if(WIN32)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    #Release支持调试set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /MANIFEST:NO")
    set(OPENSSL_ROOT_DIR ${QT_LOCATION}/../../Tools/OpenSSL/Win_${PLATFORM})
#else()
#    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -lstdc++")
#    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} -lstdc++")
endif()

if(WIN32)
    set(OS "windows")
else()
    set(OS "linux")
endif()

message(STATUS "OS=" ${OS})

set(CMAKE_PREFIX_PATH ${QT_LOCATION}/lib/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/out/${PLATFORM}/${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY=" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

if(WIN32)
    add_subdirectory(driver)
else()
    add_subdirectory(car)
endif()

add_subdirectory(base)
add_subdirectory(common) 
