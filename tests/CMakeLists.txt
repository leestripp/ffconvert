cmake_minimum_required(VERSION 3.28)

# Required for FetchContent compatibility
cmake_policy(SET CMP0135 NEW)

include(FetchContent)
fetchcontent_declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
fetchcontent_makeavailable(doctest)

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
