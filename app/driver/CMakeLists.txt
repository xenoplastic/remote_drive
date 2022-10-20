cmake_minimum_required(VERSION 3.5)

project(driver VERSION 0.1 LANGUAGES CXX)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DDEBUG)
endif()

find_package(Qt6 COMPONENTS Widgets OpenGL OpenGLWidgets LinguistTools REQUIRED)

include_directories(
    ui 
    ui/control
    server_core
    common
    ${QT_LOCATION}/../../Tools/OpenSSL/Win_${PLATFORM}/include 
    ../base
    ../common
    ${CMAKE_SOURCE_DIR}/../third_party/LogitechSteeringWheelSDK_8.75.30/include
)

link_directories(
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 
    ${QT_LOCATION}/../../Tools/OpenSSL/Win_${PLATFORM}/lib
    ${CMAKE_SOURCE_DIR}/../third_party/LogitechSteeringWheelSDK_8.75.30/Lib/${PLATFORM}
)

aux_source_directory(./ui UI_SRCS)
aux_source_directory(./server_core SERVERCORE_SRCS)
aux_source_directory(./ui/control UI_CONTROL_SRCS)
aux_source_directory(../common COMMON_SRCS)
aux_source_directory(./common SELF_COMMON_SRCS)
aux_source_directory(./res RES_SRCS)

set(
    PROJECT_SOURCES
    main.cpp
    ./res/res.rc
    ${UI_SRCS}
    ${UI_CONTROL_SRCS}
    ${COMMON_SRCS}
    ${SERVERCORE_SRCS}
    ${SELF_COMMON_SRCS}
    ${RES_SRCS}
)

set(QRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/res/res.qrc)
qt6_add_resources(QRC_FILES ${QRC_FILE})

aux_source_directory(./res/language TS_FILES)
set_source_files_properties(
    ${TS_FILES} PROPERTIES OUTPUT_LOCATION 
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language
)
qt6_add_translation(QM_FILES ${TS_FILES})

add_executable(driver WIN32
    ${PROJECT_SOURCES}
    ${QRC_FILES}
)

if(WIN32)
    target_compile_options(driver PRIVATE /wd4819 /wd4996)
endif()

target_link_libraries(driver PRIVATE 
    Qt6::Widgets 
    base 
    common 
    Qt6::OpenGLWidgets 
    LogitechSteeringWheelLib
)

set_target_properties(driver PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(driver)
endif()

#if(PLATFORM MATCHES "x86")
#    add_custom_command(
#        TARGET driver POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        ${QT_LOCATION}/../../Tools/OpenSSL/Win_x86/bin/libcrypto-1_1.dll
#        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcrypto-1_1.dll)
#else()
#    add_custom_command(
#        TARGET driver POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        ${QT_LOCATION}/../../Tools/OpenSSL/Win_x64/bin/libcrypto-1_1-x64.dll
#        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcrypto-1_1-x64.dll)
#endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set_property(TARGET driver PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") 
else()
    set_property(TARGET driver PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded:Release>")
endif()

if(WIN32)
    add_custom_command(
        TARGET driver POST_BUILD
        COMMAND ${QT_LOCATION}/bin/windeployqt
        --widgets --opengl --openglwidgets --network --websockets
        $<TARGET_FILE:driver> WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

add_custom_command(TARGET driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/driver/ui/shader/shader.vs
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shader.vs)
add_custom_command(
    TARGET driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/driver/ui/shader/shader.fs
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shader.fs)
add_custom_command(
    TARGET driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/ConfigDriver.ini 
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ConfigDriver.ini)
add_custom_command(TARGET driver POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
    ${CMAKE_SOURCE_DIR}/../third_party/LogitechSteeringWheelSDK_8.75.30/Lib/GameEnginesWrapper/${PLATFORM}/LogitechSteeringWheelEnginesWrapper.dll
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}) 
