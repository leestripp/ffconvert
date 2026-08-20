cmake_minimum_required(VERSION 3.28)

project(ffconvert VERSION 1.0.0 LANGUAGES CXX)

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
install(FILES icon/ffconvert.png DESTINATION share/icons/hicolor/1024x1024/apps COMPONENT ffconvert RENAME ffconvert.png)
install(TARGETS ${PROJECT_NAME} DESTINATION bin COMPONENT ffconvert)

# =============================================================================
# Packaging - Debian/Ubuntu .deb via CPack
#
#   cmake -S . -B build
#   cmake --build build -j"$(nproc)"
#   cmake --build build --target deb        # (or: --target package)
#
# The finished .deb is written to:  packages/deb/
# =============================================================================
set(CPACK_PACKAGE_NAME                "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR              "Lee Stripp")
set(CPACK_PACKAGE_CONTACT             "Lee Stripp <leestripp@bigpond.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple GTK4 video converter powered by FFmpeg")
set(CPACK_PACKAGE_DESCRIPTION
    "ffConvert is a straightforward GTKmm 4 application for batch-converting video files.\n Add a folder of videos, choose a modern export format (H.264, H.265/HEVC or Avid\n DNxHR), optionally scale the output, and export. All conversions are performed by\n the FFmpeg libraries. Audio is always re-encoded to AAC.")
set(CPACK_PACKAGE_HOMEPAGE_URL        "https://gitlab.com/leestripp/ffconvert")
set(CPACK_RESOURCE_FILE_LICENSE       "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README        "${CMAKE_CURRENT_SOURCE_DIR}/Readme.md")

# A .deb must install system-wide under /usr, independent of the developer's
# ~/.local CMAKE_INSTALL_PREFIX used for `cmake --install`.
set(CPACK_PACKAGING_INSTALL_PREFIX    "/usr")

# Debian/Ubuntu (DEB) generator settings.
set(CPACK_GENERATOR                   "DEB")
set(CPACK_DEBIAN_PACKAGE_SECTION      "video")
set(CPACK_DEBIAN_PACKAGE_PRIORITY     "optional")
# Automatically compute the versioned Depends: (libavcodec6x, libgtkmm-4.0-0t64, ...)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS    ON)
# Produce:  ffconvert_<version>_<arch>.deb
set(CPACK_DEBIAN_FILE_NAME            DEB-DEFAULT)

# Drop the finished .deb into ./packages/deb/
set(CPACK_OUTPUT_FILE_PREFIX          "${CMAKE_CURRENT_SOURCE_DIR}/packages/deb")

include(CPack)

# Convenience target: `make deb` rebuilds the binary, then creates the .deb.
add_custom_target(deb
    COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    DEPENDS "${PROJECT_NAME}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building Debian package -> ${CMAKE_CURRENT_SOURCE_DIR}/packages/deb/"
    VERBATIM
)
