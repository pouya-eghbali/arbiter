# Developing Arbiter

Build prerequisites, local commands and the source map for contributors. For downloads and everyday use, see the [project README](../README.md).

## Requirements

- The exact base Qt version pinned in `engine/manifest.json` (currently **6.12.0**), including Qt Quick, Quick Controls, **Qt WebEngine**,
  **Qt WebChannel**, **Qt Positioning**, **Qt SVG**, **Qt PrintSupport**, **Qt Pdf**,
  and **Qt Linguist Tools** for compiling the bundled translations.
- CMake 3.24+, Ninja, a C++20 compiler, and the Rust toolchain pinned in `dependencies.json` with Cargo.
- The current [patched WebEngine SDK](passkey-integration.md#build) for per-view ad blocking and passkeys.
- QtKeychain 0.14.2+ built with Qt 6. CMake prefers an installed package; otherwise
  it downloads and statically builds the pinned 0.14.2 archive after SHA-256
  verification. The initial configure needs network access or a local source override.
- Linux: Qt DBus, pkg-config, libsecret development headers (for example,
  `sudo apt install libsecret-1-dev pkg-config`), and a supported keyring session at runtime.
- On macOS, full Xcode 26+ with Icon Composer and `actool` to compile the layered app icon.

On macOS, use the Qt Online Installer / MaintenanceTool to install the Qt 6.12
macOS desktop kit and the Qt WebEngine, Qt WebChannel, Qt Positioning, Qt SVG, and Qt Pdf
components. WebChannel and Positioning are required by WebEngine. A Homebrew Qt installation
without WebEngine is insufficient. Use components from the same Qt installation;
do not mix the Homebrew libraries with an older Online Installer WebEngine.

After installing or upgrading Xcode, open it to complete any required setup and
review and accept its license before building. If the compiler reports an
unaccepted license, run `sudo xcodebuild -license` in Terminal and follow the
prompts.

## Build and run

The Svelte website lives in [website/](../website/README.md). It has its own Node.js
toolchain and static build; it is independent of the browser's CMake build.

Build the current engine patches first using `engine/build.py` (see
[engine build instructions](passkey-integration.md)). Then, for the standard
Qt Online Installer location and SDK installed at `build-passkey-sdk`:

```sh
cmake -S . -B build-qt612 -G Ninja \
  -DQt6_DIR="$HOME/Qt/6.12.0/macos/lib/cmake/Qt6" \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DARBITER_PATCHED_WEBENGINE=ON \
  -DARBITER_WEBENGINE_PREFIX="$PWD/build-passkey-sdk"
cmake --build build-qt612 --parallel 24
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


## Source layout

- `src/main.cpp`: application identity, Qt WebEngine initialization, QML loading.
- `src/macwindow.mm`: immersive macOS window setup with native corners and shadow.
- `src/keybindings.*`: shortcut model, validation, defaults, and saved preferences.
- `src/tabs.*`: tab identity, selection, metadata, and recently closed URLs.
- `src/passwordmanager.*`: credential index, save/update flow, and per-tab fill authorization.
- `src/secretstore.*`: asynchronous QtKeychain adapter with plaintext fallback disabled.
- `scripts/passwords.js`: isolated main-frame form detection, capture, and fill.
- `qml/PasswordPanel.qml`: account picker, prompts, generator, and management UI.
- `engine/`: pinned Qt/Chromium patches and isolated SDK build tooling.
- `src/passkey*`: encrypted QtKeychain vault, OpenSSL authenticator operations and native transport UI.
- `src/webauthncontroller.*` and `qml/WebAuthnPanel.qml`: standard Qt WebAuthn request UI and lifecycle.
- `src/browserapplication.h`: receives and queues native HTTP/HTTPS open events.
- `cmake/Info.plist.in`: macOS bundle metadata and URL-scheme registration.
- `assets/icon/Arbiter.icon`: editable Icon Composer source; the macOS build compiles its Liquid Glass asset catalog and compatibility icon.
- `qml/Main.qml`: minimal browser window and navigation.
- `CMakeLists.txt`: Qt dependencies and application bundle.

## Tests

Routine GitHub CI installs **prebuilt stock Qt**, including Qt PDF. It never
builds Chromium. Stock-compatible unit, credential-store and browser tests run
there; tests of Arbiter's compositor, extension bridge and cross-frame permission
patches require the patched SDK. Configure with `ARBITER_PATCHED_WEBENGINE=ON`
and `ARBITER_WEBENGINE_PREFIX` to include those checks. The local/native release
builder does this and runs the complete suite against its matching SDK.
Routine CI jobs have a 30-minute limit and individual tests a five-minute limit.
Unchanged C++ compilation is cached with sccache. Windows CI has a stricter
two-minute test timeout. It compiles every stock-compatible test target, then
runs unit/UI-model tests and the native credential-store checks. Tests labeled
`webengine-runtime` run on macOS/Linux CI and in the complete local/native SDK
suite, including Windows. Audio policy also has a standalone Windows unit fixture.

The stock Windows WebEngine 6.140 binary imports an accessibility constructor
that Qt 6.12's GUI library no longer exports under its old MSVC name. A diagnostic
import-name repair allowed loading, but browser transfer tests still crashed on
the hosted runner. CI does not alter or execute those incompatible stock modules.
Windows browser integration must pass against the matching patched SDK before
publishing a Windows package. The local release builder runs all tests without
the CI label exclusion. The hosted exclusion applies only to that exact Qt and
WebEngine version pair; dependency upgrades re-enable the full Windows suite.
CI and native release Qt kits have separate caches.

Run the keyboard binding, native URL handling, tab model, password controller,
WebEngine password-form integration, passkey vault, and WebAuthn controller tests
(OpenSSL 3 development files are required):

```sh
cmake -S tests -B build-tests -G Ninja \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos"
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

The actual OS credential-store round trip is opt-in: run with
`ARBITER_TEST_KEYCHAIN=1` in an unlocked desktop session. It uses a disposable
synthetic credentials, including encrypted passkey registration/assertion. The
patched-engine browser fixture skips on stock Qt. On macOS use Cocoa, not the offscreen platform plugin,
because QtKeychain completes operations on Apple's main dispatch queue.
The cross-platform workflow is [Desktop checks](../.github/workflows/password-manager.yml).


## Site compatibility

`SiteUserAgentInterceptor`, installed on every workspace/private profile, sends
a Firefox User-Agent header for HTTP(S) requests whose destination is
`google.com` or one of its subdomains. This follows the
[qutebrowser Google sign-in workaround](https://github.com/qutebrowser/qutebrowser/issues/5182).
It clears Chromium UA client hints present on those requests. A document-creation
script also matches the JavaScript-visible browser identity on Google pages,
including subframes, before page scripts run. Other sites and the shared profile's
default user agent remain unchanged. This compatibility workaround does not turn
Chromium into Firefox or guarantee Google sign-in acceptance; workers retain the
engine's native identity. It remains active when ad blocking is paused. The Firefox version is pinned
in `src/siteuseragent.h`, with its verification date and Mozilla release source.
Regression tests check request headers, early page scripts, and profile script
installation. Request tests block requests before network access.

## Further reading

- [Releases and dependency maintenance](releases.md)
- [Release update checks](updates.md)
- [Patched engine and passkeys](passkey-integration.md)
- [Password manager](password-manager.md)
- [Media integration](media-integration.md)
- [Settings example](settings.example.toml)
