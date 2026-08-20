cmake_minimum_required(VERSION 3.28)

# FetchContent but don't install doctest
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
)
# doctest is only needed to BUILD the unit tests - never install it.
# Without this its headers and CMake package files leak into
# `cmake --install` (~/.local) and into the CPack .deb.
set(DOCTEST_NO_INSTALL ON)
FetchContent_MakeAvailable(doctest)

add_executable(test_ffconvert
    test_channel_layout.cpp
)

target_link_libraries(test_ffconvert
    PRIVATE
    doctest::doctest
    PkgConfig::AVCODEC
    PkgConfig::AVFORMAT
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::SWRESAMPLE
)

target_include_directories(test_ffconvert PRIVATE ${CMAKE_SOURCE_DIR}/src)