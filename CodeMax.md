# CodeMax.md - Scale Option Implementation

## Project Info
- **Project**: ffConvert - GTK4-based video converter using FFmpeg
- **Build System**: CMake
- **Language**: C++20
- **Key Dependencies**: GTKmm 4, FFmpeg libraries (libavcodec, libavformat, libavutil, libswscale, libswresample)

## Tasks

### Task 1: Update ffconvert.h - Add scale parameters ✅
- Modify `convert_video_file` function signature to accept optional width/height parameters
- Add default values (0 = no scaling)

### Task 2: Update ffconvert.cpp - Implement scaling logic ✅
- Modify setup_output_file to use provided dimensions instead of source dimensions when scale is enabled
- Change SWS_BILINEAR to SWS_LANCZOS for better quality scaling (best quality, not too soft)
- Ensure dimensions are even (required by codecs like H.264/H.265)

### Task 3: Update MainWindow.h - Add scale UI widgets ✅
- Add Gtk::CheckButton for scale enable/disable
- Add Gtk::Entry for width input
- Add Gtk::Entry for height input
- Add Gtk::Label for "Scale" section
- Add member variables to store scale settings
- Add signal handler for checkbox

### Task 4: Update MainWindow.cpp - Add scale UI and pass values ✅
- Create and layout the scale widgets in the right pane
- Add signal handler to toggle width/height widgets based on checkbox
- Get scale values when exporting
- Pass scale values to FFConvert::convert_video_file

## Implementation Details
- Scale checkbox: "Enable Scale"
- Width/Height entries: Number inputs with placeholder text
- Default scale algorithm: SWS_LANCZOS (high quality, less soft than bilinear)
- When scale is disabled: Use original dimensions (current behavior)
- Dimensions are enforced to be even numbers (required by video codecs)

## Summary
Successfully implemented the scale option with:
1. A checkbox to enable/disable scaling
2. Width and height input fields that become active when checkbox is checked
3. High-quality Lanczos scaling algorithm (better than bilinear, not too soft)
4. Proper dimension validation (ensures even dimensions for codec compatibility)
