# Debian/Ubuntu Package (`.deb`)

This directory is the **output location** for the ffConvert Debian/Ubuntu
package. The packaging itself is driven by **CPack** (configured at the bottom
of the top-level `CMakeLists.txt`), so there are no hand-maintained `control`
files here — the `.deb`, its `control` metadata, its dependency list, and its
file layout are all generated automatically from the build.

## What goes into the package

The contents come from the existing CMake `install()` rules:

| Source                        | Installed to (in the `.deb`)                     |
|-------------------------------|--------------------------------------------------|
| `ffconvert` binary            | `/usr/bin/ffconvert`                             |
| `resource/ffconvert.desktop`  | `/usr/share/applications/ffconvert.desktop`      |
| `icon/ffconvert.png`          | `/usr/share/icons/hicolor/1024x1024/apps/`       |

Because a `.deb` installs system-wide, CPack is configured with
`CPACK_PACKAGING_INSTALL_PREFIX=/usr`, **independent** of the developer
`~/.local` prefix used by `cmake --install`.

Runtime dependencies (`Depends:`) are computed automatically with
`dpkg-shlibdeps` (`CPACK_DEBIAN_PACKAGE_SHLIBDEPS=ON`), so the correct
versioned library packages (e.g. `libavcodec6x`, `libgtkmm-4.0-0t64`, ...)
are pulled in for the system you build on.

## Building the package

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
cmake --build build --target deb        # or: --target package
```

The finished package is written here, e.g.:

```
packages/deb/ffconvert_1.0.0_amd64.deb
```

The version number comes from `project(ffconvert VERSION ...)` in
`CMakeLists.txt` — bump it there to version the package.

## Installing / inspecting

```bash
# Install
sudo dpkg -i packages/deb/ffconvert_1.0.0_amd64.deb
sudo apt-get install -f        # pull in any missing dependencies

# Inspect without installing
dpkg-deb --info     packages/deb/ffconvert_1.0.0_amd64.deb
dpkg-deb --contents packages/deb/ffconvert_1.0.0_amd64.deb
```

## Note

Built `*.deb` files are **build artifacts** and are excluded from git (see
`.gitignore`). This README keeps the otherwise-empty directory under version
control.
