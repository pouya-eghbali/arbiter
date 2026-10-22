# Arbiter

A distraction-free desktop browser built with C++, QML, and Qt WebEngine (Chromium).

The project's primary branch is `master`.

The browser has tabs, an address/search field, back/forward,
reload/stop, loading progress, and a persistent local browser profile. It starts
on a blank page without making a startup network request. Search queries go to
DuckDuckGo. Workspaces, extension management, downloads, and password/autofill
integration are not implemented yet.

The page fills the window. On macOS, the native frame retains rounded corners
and shadows while its title text, background, and window buttons are hidden.
There is no persistent toolbar or address bar over websites. Cmd+L opens a
centered address editor with the full URL selected; Enter navigates, and Escape
or a click outside dismisses the editor. The default empty page has a centered
address/search field, focused on startup. Drag the empty page background to move
the window.

The macOS bundle registers HTTP and HTTPS URL schemes. Links opened with
`open -a /path/to/arbiter.app 'https://example.com/'` are delivered to the
existing window, including when the app is launched by the link. Registration
does not make Arbiter the default browser.

Each tab keeps its page and navigation history while you switch between tabs.
Cmd+Shift+A opens a searchable grid of page previews with titles and addresses;
use the arrow keys and Enter to select a tab, click a card, or use its close button. The start page and native Tabs menu
also open the switcher. Websites requesting a new tab open separately, and
external HTTP/HTTPS links open a new tab unless the active tab is empty.
Tabs share the browser's cookie profile. Open tabs are not restored after
quitting yet; reopening a closed tab restores its latest URL, not its form
contents or full navigation history.

Open Settings from the start page or the native Arbiter > Preferences… menu,
with Cmd+, on macOS / Ctrl+, elsewhere, or by
entering `arbiter://settings` in the address editor. Click a binding and press a
new key combination. Bindings save immediately, duplicate assignments are
rejected, and Reset to defaults restores the original bindings.

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

- Cmd+L on macOS / Ctrl+L elsewhere: open the address editor, or focus the empty
  page's address field.
- Platform refresh, back, and forward shortcuts; Escape stops loading.
- Escape dismisses the address editor without navigating.
- Cmd+T opens a new tab; Cmd+W closes the active tab (or the window for the last tab).
- Cmd+Shift+A opens the tab switcher; Cmd+Shift+T reopens a closed tab.
- Control+Tab / Control+Shift+Tab select the next / previous tab on macOS.
- Cmd+Shift+W closes the window; Cmd+Ctrl+F toggles full screen.
- Cmd+Ctrl+M expands the window to fill the desktop, and restores its previous
  size when pressed again. This is separate from full screen.
- Option-drag anywhere in the macOS window moves it. Normal page dragging is
  unaffected, and moving is disabled while in full screen.
- Drag the top 10 pixels of the window to move it over a website; a small handle
  appears on hover. Double-click that edge to expand or restore the window.
- Enter a URL, hostname, or search query in the address field.

Keyboard shortcuts above are defaults and can be changed in Settings.

Browser cookies and cache use Qt's platform-local application data/cache paths
under the `Arbiter` application identity and `Default` profile. This is not a
password vault or a sync service. Chromium's sandbox and normal TLS certificate
validation remain enabled.

The browser uses Qt WebEngine's Chromium user agent with the QtWebEngine and
Arbiter product tokens removed for website compatibility. Chromium's real
version and platform tokens are preserved. JavaScript, workers, WebAssembly,
and Web Crypto use the engine's normal settings; no extra JavaScript sandbox
or script blocker is added.

For optional page-console, loading, engine-version, and browser-feature
diagnostics, launch the executable directly:

```sh
ARBITER_DIAGNOSTICS=1 ./build-qt612/arbiter.app/Contents/MacOS/arbiter
```

## Source layout

- `src/main.cpp`: application identity, Qt WebEngine initialization, QML loading.
- `src/macwindow.mm`: immersive macOS window setup with native corners and shadow.
- `src/keybindings.*`: shortcut model, validation, defaults, and saved preferences.
- `src/tabs.*`: tab identity, selection, metadata, and recently closed URLs.
- `src/browserapplication.h`: receives and queues native HTTP/HTTPS open events.
- `cmake/Info.plist.in`: macOS bundle metadata and URL-scheme registration.
- `qml/Main.qml`: minimal browser window and navigation.
- `CMakeLists.txt`: Qt dependencies and application bundle.

## Tests

Run the keyboard binding, native URL handling, and tab model tests independently of the browser:

```sh
cmake -S tests -B build-tests -G Ninja \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos"
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```
