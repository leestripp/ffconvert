# ffConvert

A simple, intuitive GTK4-based video converter for Linux, powered by the robust FFmpeg libraries.


## About

ffConvert is designed to be a straightforward tool for batch-converting video files. It provides a clean graphical user interface built with GTKmm 4, allowing users to quickly add a folder of videos, select a modern export format, and convert them to a new location. The heavy lifting is done by FFmpeg, ensuring high-quality and reliable conversions.

## Features

*   **Simple Interface:** A clean, two-pane layout for easy file management and export configuration.
*   **Batch Processing:** Add an entire folder of video files to the conversion queue with a single click.
*   **Modern Formats:** Convert to widely-supported and professional codecs:
    *   H.264 (MP4)
    *   H.265 / HEVC (MP4)
    *   Avid DNxHR HQX 10-bit 4:2:2 (MOV)
*   **Progress Tracking:** A progress bar keeps you updated on the overall status of the batch export.

## Dependencies

To build ffConvert from source, you will need the following development packages installed on your system:

*   A C++20 compatible compiler (e.g., GCC 10+ or Clang 12+)
*   CMake (version 3.16 or higher)
*   `pkg-config`
*   GTKmm 4 (`libgtkmm-4.0-dev`)
*   FFmpeg libraries (version 5.0 or newer recommended):
    *   `libavcodec-dev`
    *   `libavformat-dev`
    *   `libavutil-dev`
    *   `libswscale-dev`
    *   `libswresample-dev`
