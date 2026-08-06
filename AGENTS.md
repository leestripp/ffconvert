# AGENTS.md — ffConvert

Guidance for AI coding agents working in this repository.

## Project Overview

ffConvert is a simple GTK4-based batch video converter for Linux. The user adds a
folder of video files, picks an export format and destination, and the app
transcodes every file using the FFmpeg C libraries directly (not the ffmpeg CLI).

Export formats offered in the UI:

- H.264 (MP4) — `libx264`, audio re-encoded to AAC
- H.265 / HEVC (MP4) — `libx265`, audio to AAC
- Avid DNxHR HQX 10-bit 4:2:2 (MOV) — `dnxhd` encoder, audio to AAC

The GUI is GTKmm 4 (two-pane layout: file list on the left, export settings on
the right). Conversion runs on a worker thread; progress/completion are
signalled back to the GTK main thread via `Glib::Dispatcher`.

## Tech Stack

| Component        | Details                                                        |
|------------------|----------------------------------------------------------------|
| Language         | C++20 (`-Wall -Wextra`; `-O3` Release, `-g` Debug)             |
| Build system     | CMake ≥ 3.28 (developed with CMake 4.x), Ninja or Make         |
| GUI              | GTKmm 4 (`gtkmm-4.0`, via pkg-config)                          |
| Media            | FFmpeg libs: libavcodec, libavformat, libavutil, libswscale, libswresample |
| Tests            | doctest v2.4.12 (fetched via CMake FetchContent)               |
| Static analysis  | cppcheck, run automatically during builds if installed         |
| OS               | Linux (GTK4; tested on Wayland)                                |

Dependencies on Debian/Ubuntu: `libgtkmm-4.0-dev`, `libavcodec-dev`,
`libavformat-dev`, `libavutil-dev`, `libswscale-dev`, `libswresample-dev`,
plus a C++20 compiler, CMake, and pkg-config. `libswscale-dev` is the one most
commonly missing.

## Build, Test, Install

```bash
# Configure + build
cmake -S . -B build -G Ninja
cmake --build build -j"$(nproc)"

# Run tests — IMPORTANT: `ctest` finds nothing because tests/CMakeLists.txt
# never calls add_test(). Run the test binary directly:
./build/tests/test_ffconvert

# Install (prefix defaults to ~/.local). Use the ffconvert component to install
# only the app files and skip the fetched doctest headers:
cmake --install build --component ffconvert

# Run the app (needs a display; use GDK_BACKEND=wayland on Wayland)
./build/ffconvert
```

Expected test result: 7 test cases / 34 assertions, all passing.

## Project Structure

```
├── CMakeLists.txt            # Top-level build: deps via pkg-config, install rules (component "ffconvert")
├── Readme.md                 # User-facing readme
├── src/
│   ├── main.cpp              # Gtk::Application entry point (app id com.example.ffconvert)
│   ├── MainWindow.h/.cpp     # All GTK UI: layout, signal handlers, export worker thread
│   └── ffconvert.h/.cpp      # FFConvert::convert_video_file() — the FFmpeg transcode pipeline
├── tests/
│   ├── CMakeLists.txt        # doctest via FetchContent (pinned to v2.4.12 — see Gotchas)
│   └── test_channel_layout.cpp  # doctest unit tests (FFmpeg channel-layout/codec sanity)
├── resource/ffconvert.desktop  # Desktop entry (Icon=ffconvert, theme lookup)
└── icon/                     # ffconvert.png (installed to hicolor/1024x1024/apps), banner, 192px jpg
```

## Architecture Notes

- `ffconvert.cpp` keeps all implementation details in an **anonymous namespace**.
  The public API is just `FFConvert::convert_video_file(in, out, format_name,
  scale_w, scale_h)`; `format_name` is matched with `std::string::find` against
  the UI dropdown strings ("H.264", "H.265", "Avid DNxHR", "(MP4)", "(MOV)"), so
  dropdown text and the matching logic must stay in sync.
- Pipeline: `open_input_file` → `setup_output_file` → `run_conversion_loop` →
  `flush_encoders` → `av_write_trailer`. Only the first video and first audio
  stream are transcoded; other streams are dropped.
- RAII: `ConversionContext` frees all FFmpeg resources in its destructor;
  frames/packets are held in `unique_ptr` with custom deleters
  (`AVFramePtr`/`AVPacketPtr`).
- Video scaling/pixel-format conversion via `sws_scale` (SWS_LANCZOS); audio is
  resampled with libswresample into an `AVAudioFifo` and fed to the AAC encoder
  in fixed `frame_size` chunks.
- Threading (MainWindow): `export_thread_worker()` runs conversions on
  `std::thread m_export_thread`; UI is updated only from the main thread after
  `Glib::Dispatcher` emits. Export-job data is copied into `m_files_to_export`
  etc. before the thread starts; `m_files_processed_count` is `std::atomic<int>`.

## Code Style and Conventions

- Headers use `#pragma once`; project includes are `"Header.h"` form.
- FFmpeg C headers are always wrapped in `extern "C" { ... }`.
- GTK member widgets use the `m_` prefix (`m_exportButton`), as do all
  MainWindow members; local FFmpeg contexts use snake_case (`dec_ctx`, `enc_ctx`).
- Functions are snake_case; classes are PascalCase.
- Error handling: functions return `bool`, errors are printed to `std::cerr`
  (FFmpeg errors via the `log_error()` helper); progress goes to `std::cout`.
- Keep C-style FFmpeg calls out of the UI layer — all libav* usage belongs in
  `ffconvert.cpp`.
- 4-space indentation (tabs appear in a few legacy spots; prefer spaces in new code).

## Gotchas and Hard-Won Lessons

- **Video PTS must be generated from a monotonic counter** (`next_video_pts++`
  in `StreamContext`), never rescaled from source timestamps. WMV/ASF tape
  captures have jittery millisecond timestamps whose rescale into the coarse
  1/fps encoder time base produces duplicate/backward PTS, and the MP4/MOV
  muxer aborts the whole conversion with EINVAL. Do not regress this.
- **doctest is pinned to v2.4.12** — v2.4.11's `cmake_minimum_required` is
  rejected by CMake 4.x.
- **`ctest` reports "No tests were found"** — run `./build/tests/test_ffconvert`
  directly.
- AAC encoder context is configured with `av_channel_layout_default()` +
  `AV_SAMPLE_FMT_FLTP` on purpose: some AVI/v4l2 sources have channel layouts
  AAC rejects. Tests in `test_channel_layout.cpp` guard this.
- The input file filter lives in `MainWindow::add_videos_from_folder()` (a set
  of lowercase extensions, `.wmv` included). Add new formats there too.
- Install rules: the `.desktop` file uses `Icon=ffconvert` (no extension) for
  theme lookup; the PNG installs to `share/icons/hicolor/1024x1024/apps/`.
- `.daedalus/` is agent-runtime state — never touch, delete, or commit it
  (it's already in `.gitignore`).

## Git Workflow

- Single `main` branch, pushed to `origin` (GitLab: leestripp/ffconvert).
  Topic branches are merged (fast-forward) and deleted after use.
- Commit messages: short imperative summary, blank line, bullet details.
