cmake_minimum_required(VERSION 3.28)

# FetchContent but don't install doctest
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
)
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