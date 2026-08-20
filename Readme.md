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
*   **Optional Scaling:** Downscale or upscale the output to a custom width and height.
*   **Progress Tracking:** A progress bar keeps you updated on the overall status of the batch export.

## Dependencies

To build ffConvert from source, you will need the following development packages installed on your system:

*   A C++20 compatible compiler (e.g., GCC 10+ or Clang 12+)
*   CMake (version 3.28 or higher)
*   `pkg-config`
*   GTKmm 4 (`libgtkmm-4.0-dev`)
*   FFmpeg libraries (version 5.0 or newer recommended):
    *   `libavcodec-dev`
    *   `libavformat-dev`
    *   `libavutil-dev`
    *   `libswscale-dev`
    *   `libswresample-dev`

On Debian/Ubuntu, install them all with:

```bash
sudo apt install build-essential cmake pkg-config libgtkmm-4.0-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
```

## Building from Source

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Installation

ffConvert installs into `~/.local` by default (no root required):

```bash
cmake --install build --component ffconvert
```

This installs the binary to `~/.local/bin`, the desktop entry to
`~/.local/share/applications`, and the application icon into the hicolor icon
theme. Make sure `~/.local/bin` is on your `PATH`.

## Packaging (Debian/Ubuntu `.deb`)

ffConvert can build a system-wide Debian/Ubuntu package with
[CPack](https://cmake.org/cmake/help/latest/module/CPack.html) (the packaging
config lives at the bottom of `CMakeLists.txt`). This is separate from the
`~/.local` developer install above: the `.deb` installs into `/usr` so it
integrates with the desktop environment for all users.

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
cmake --build build --target deb        # or: --target package
```

The finished package is written to `packages/deb/`, e.g.
`packages/deb/ffconvert_1.0.0_amd64.deb`. It contains the binary
(`/usr/bin/ffconvert`), the desktop entry
(`/usr/share/applications/ffconvert.desktop`), and the application icon in the
hicolor theme. Runtime dependencies (`Depends:`) are computed automatically
with `dpkg-shlibdeps`, so the correct versioned FFmpeg/GTKmm library packages
are required at install time.

The package version comes from `project(ffconvert VERSION ...)` in
`CMakeLists.txt` — bump it there to version a release.

Install and inspect with:

```bash
sudo dpkg -i packages/deb/ffconvert_1.0.0_amd64.deb
sudo apt-get install -f        # resolve any missing dependencies

dpkg-deb --info     packages/deb/ffconvert_1.0.0_amd64.deb
dpkg-deb --contents packages/deb/ffconvert_1.0.0_amd64.deb
```

Built `*.deb` files are build artifacts and are excluded from git via
`.gitignore`. See `packages/deb/README.md` for more detail.

## Usage

1.  Launch `ffconvert` from your application menu or the command line.
2.  Click **Add Folder...** and choose a folder containing video files — all
    supported videos in that folder are added to the queue (MP4, MKV, AVI,
    WMV, MOV, MPEG, FLV, WebM, and many more).
3.  Select an export format from the drop-down (H.264, H.265, or Avid DNxHR).
4.  Optionally tick **Enable Scale** and enter a custom output width/height.
5.  Click **Browse...** to choose where the converted files are written.
6.  Click **Export** and watch the progress bar.

Audio is always re-encoded to AAC. Converted files keep their original name
with the new extension (`.mp4` or `.mov`).

## Running Tests

Unit tests use the [doctest](https://github.com/doctest/doctest) framework,
fetched automatically by CMake:

```bash
./build/tests/test_ffconvert
```

## License

ffConvert is released under the [MIT License](LICENSE).
