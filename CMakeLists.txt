cmake_minimum_required(VERSION 3.20)
project(volsa2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find system libraries via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)
pkg_check_modules(SAMPLERATE REQUIRED samplerate)
pkg_check_modules(SNDFILE REQUIRED sndfile)

# Core Library
add_library(volsa2_core STATIC
    src/seven_bit.cpp
    src/proto.cpp
    src/audio.cpp
    src/device.cpp
    src/sample_chopper.cpp
)

target_include_directories(volsa2_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${ALSA_INCLUDE_DIRS}
    ${SAMPLERATE_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(volsa2_core PUBLIC
    ${ALSA_LIBRARIES}
    ${SAMPLERATE_LIBRARIES}
    ${SNDFILE_LIBRARIES}
)

# CLI Binary
add_executable(volsa2-cli cli/main.cpp)
target_link_libraries(volsa2-cli PRIVATE volsa2_core)

# Tests
enable_testing()

add_executable(test_seven_bit tests/test_seven_bit.cpp)
target_link_libraries(test_seven_bit PRIVATE volsa2_core)
add_test(NAME test_seven_bit COMMAND test_seven_bit)

add_executable(test_proto tests/test_proto.cpp)
target_link_libraries(test_proto PRIVATE volsa2_core)
add_test(NAME test_proto COMMAND test_proto)

add_executable(test_audio tests/test_audio.cpp)
target_link_libraries(test_audio PRIVATE volsa2_core)
add_test(NAME test_audio COMMAND test_audio)

add_executable(test_proto_comprehensive tests/test_proto_comprehensive.cpp)
target_link_libraries(test_proto_comprehensive PRIVATE volsa2_core)
add_test(NAME test_proto_comprehensive COMMAND test_proto_comprehensive)

add_executable(test_sample_chopper tests/test_sample_chopper.cpp)
target_link_libraries(test_sample_chopper PRIVATE volsa2_core)
add_test(NAME test_sample_chopper COMMAND test_sample_chopper)

# GUI
option(BUILD_GUI "Build Qt GUI application" ON)

if (BUILD_GUI)
    list(APPEND CMAKE_PREFIX_PATH "/usr/local/Qt-6.9.1")
    find_package(Qt6 COMPONENTS Core Gui Widgets Multimedia)
    if (Qt6_FOUND)
        message(STATUS "Qt6 found (${Qt6_VERSION}). Building Qt GUI.")
        add_subdirectory(gui)
    else()
        message(WARNING "Qt6 not found. Skipping GUI build.")
    endif()
endif()
