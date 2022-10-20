cmake_minimum_required(VERSION 3.5)

project(common LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Widgets WebSockets Network REQUIRED)

aux_source_directory(. SELF_SRCS)

add_library(common STATIC 
    ${SELF_SRCS}
)

target_link_libraries(common PUBLIC Qt6::Widgets Qt6::WebSockets Qt6::Network)

if(WIN32)
        target_compile_options(common PUBLIC /wd4819 /wd4996)
endif()
