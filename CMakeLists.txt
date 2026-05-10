cmake_minimum_required(VERSION 3.28)

project(ffconvert LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Installation prefix" FORCE)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Dependencies
# Use PkgConfig to find our libraries, which is the standard on Linux.
find_package(PkgConfig REQUIRED)

# Static analysis with cppcheck
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    set(CMAKE_CXX_CPPCHECK
        ${CPPCHECK}
        "--suppress=missingInclude"
        "--suppress=cstyleCast"
        "--suppress=useStlAlgorithm"
        "--suppress=unusedFunction"
    )
endif()

# Find GTKmm 4
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)

# Find FFmpeg libraries
# We need avcodec, avformat, avutil, swscale, and swresample for audio.
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)


add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.cpp
    src/ffconvert.cpp
)

# Link Libraries
# Link all the libraries we found to our executable.
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    PkgConfig::GTKMM
    PkgConfig::AVCODEC
    PkgConfig::AVFORMAT
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::SWRESAMPLE
)

# Set include directories for our own project headers
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Unit tests
add_subdirectory(tests)

# install
install(FILES resource/ffconvert.desktop DESTINATION share/applications COMPONENT ffconvert)
install(FILES icon/ffconvert.svg DESTINATION share/icons/hicolor/scalable/apps COMPONENT ffconvert)
install(FILES icon/ffconvert.svg DESTINATION share/icons COMPONENT ffconvert)
install(TARGETS ${PROJECT_NAME} DESTINATION bin COMPONENT ffconvert)
