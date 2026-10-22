# Arbiter

A distraction-free desktop browser built with C++, QML, and Qt WebEngine (Chromium).

The project's primary branch is `master`.

This initial scaffold has a single web view, address/search field, back/forward,
reload/stop, loading progress, and a persistent local browser profile. It starts
on a blank page without making a startup network request. Search queries go to
DuckDuckGo. Workspaces, tabs, extension management, downloads, and password/autofill
integration are not implemented yet.

## Requirements

- Qt **6.12.0 or newer**, including Qt Quick, Quick Controls, **Qt WebEngine**,
  **Qt WebChannel**, and **Qt Positioning**.
- CMake 3.24+, Ninja, and a C++20 compiler (Xcode on macOS).

On macOS, use the Qt Online Installer / MaintenanceTool to install the Qt 6.12
macOS desktop kit and the Qt WebEngine, Qt WebChannel, and Qt Positioning
components. WebChannel and Positioning are required by WebEngine. A Homebrew Qt installation
without WebEngine is insufficient. Use components from the same Qt installation;
do not mix the Homebrew libraries with an older Online Installer WebEngine.

After installing or upgrading Xcode, open it to complete any required setup and
review and accept its license before building. If the compiler reports an
unaccepted license, run `sudo xcodebuild -license` in Terminal and follow the
prompts.

## Build and run

For the standard Qt Online Installer location:

```sh
cmake -S . -B build-qt612 -G Ninja \
  -DQt6_DIR="$HOME/Qt/6.12.0/macos/lib/cmake/Qt6" \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-qt612 --parallel
open build-qt612/arbiter.app
```

Adjust `Qt6_DIR` and `CMAKE_PREFIX_PATH` to your complete Qt kit. Use a fresh build
directory when switching Qt installations: an existing CMake cache can retain
paths to Homebrew Qt even when `CMAKE_PREFIX_PATH` changes. In Qt Creator, open
`CMakeLists.txt` and select that same desktop kit. For console diagnostics, run
`./build-qt612/arbiter.app/Contents/MacOS/arbiter` directly. On Linux the executable
is `./build-qt612/arbiter`; on Windows it is `build-qt612/arbiter.exe` (or the configuration
subdirectory with a multi-configuration generator).

To lint QML after configuration:

```sh
cmake --build build-qt612 --target arbiter_qmllint
```

The development build uses the installed Qt runtime. Shipping a standalone app
will require deployment of Qt libraries, QML plugins, WebEngine resources and
helpers, followed by platform signing/notarization.

## Controls

- Cmd+L on macOS / Ctrl+L elsewhere: focus the address field.
- Platform refresh, back, and forward shortcuts; Escape stops loading.
- Enter a URL, hostname, or search query in the address field.

Browser cookies and cache use Qt's platform-local application data/cache paths
under the `Arbiter` application identity and `Default` profile. This is not a
password vault or a sync service. Chromium's sandbox and normal TLS certificate
validation remain enabled.

## Source layout

- `src/main.cpp`: application identity, Qt WebEngine initialization, QML loading.
- `qml/Main.qml`: minimal browser window and navigation.
- `CMakeLists.txt`: Qt dependencies and application bundle.

## Verified build (2026-10-22)

The commands above successfully configured and built the macOS app using Qt
6.12.0 from the Qt Online Installer, AppleClang 21.0.0 from Xcode, CMake, and
Ninja. The `arbiter_qmllint` target passed with that same Qt installation. The
app was launched and its process was confirmed running. Navigation and other
browser interactions have not yet been checked as part of this validation.
