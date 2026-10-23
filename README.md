# Arbiter

A distraction-free desktop browser built with C++, QML, and Qt WebEngine (Chromium).

The project's primary branch is `master`.

The browser has tabs, an address/search field, back/forward,
reload/stop, loading progress, a persistent local browser profile, and a local
password manager backed by QtKeychain. It starts
on a blank page without making a startup network request. Search queries go to
DuckDuckGo. Workspaces, extension management, downloads, and credential sync
are not implemented yet. An opt-in patched-engine integration adds Arbiter
passkeys and Chromium's phone QR transport. The custom runtime builds on Apple
Silicon macOS and passes browser passkey registration/sign-in tests; phone
sign-in on GitHub was also confirmed manually with an iPhone. Windows/Linux
runtime validation remains pending. See
[passkey integration](docs/passkey-integration.md) for build instructions and
current limitations. Stock Qt builds keep those features disabled.
Standard Qt WebAuthn requests have native account-selection, security-key PIN,
error, and cancellation UI; actual authenticator availability depends on Qt/OS.

The page fills the window. On macOS, the native frame retains rounded corners
and shadows while its title text, background, and window buttons are hidden.
The Glacier interface uses frosted overlays with subtle borders and defaults to
the system color theme. Settings → Appearance offers System, Dark and Light;
the selection is saved and System follows changes to the OS appearance.
Settings uses vertical category tabs for Appearance, Passwords, Passkeys and
Keyboard. Checkboxes, dropdown menus and navigation tooltips share rounded,
theme-aware styling.
Appearance saves independent Light and Dark accent and background colors. Choose
a preset or open the centered in-app color picker; Reset restores each color's default.
Background colors tint the browser surfaces, with foreground colors adapting for
readability. The color picker applies changes on confirmation; Cancel preserves
the existing color.
Buttons, fields and other shared controls live in [`qml/ui`](qml/ui/README.md)
and are reused across Settings, navigation and credential dialogs.
A bottom navigation pill expands on hover or click, then shrinks to a tiny
handle after the pointer leaves. It stays open while using its keyboard controls
or menu. Settings → Appearance → Show navigation pill turns it off completely;
keyboard shortcuts and native menus remain available.
On macOS, Appearance → Website navigation in the menu bar offers Tab title
(the default), Top-level navigation, or Off. Arbiter reads visible links from
the active page's `<nav>` and `[role="navigation"]` regions, including nested
list navigation, and opens selected links in that tab. Menus refresh after
navigation and tab switches, and every two seconds for dynamic pages. Up to six
navigation headings appear directly in the menu bar; additional headings go in
More. Pages without navigation links show a disabled placeholder in the tab-title
menu. Script-only buttons, hidden dropdown contents and frame navigation are
not included.
Settings → Passkeys offers optional biometric unlocking for Arbiter's own vault:
Touch ID on macOS, Windows Hello on Windows 11, and enrolled fprintd fingerprint
readers on Linux. Windows Hello may offer its PIN as well as biometrics.
Enable it by entering the vault passphrase and completing device verification.
After a successful passphrase unlock, each later passkey operation can use fresh
device verification for up to 15 minutes while Arbiter stays active. Restarting,
leaving the app, locking/sleeping the session, expiration, or Lock now clears the memory-only session key.
Native verification dialogs may briefly take focus while authenticating.
The passphrase remains available as a fallback, including after cancellation or
failed verification; no persistent biometric bypass key or passphrase is stored.
Unsupported/unconfigured devices keep the passphrase flow. Windows builds need
MSVC and a Windows 11 SDK; Linux builds need Qt DBus, with fprintd configured by
the desktop. See [biometric details](docs/passkey-integration.md#biometric-session-unlocking).
History is available from History → Show All History, the navigation pill's
History item, or Cmd+Y on macOS / Ctrl+Y elsewhere. The shortcut can be changed
in Settings → Keyboard. The searchable history view groups visits by date,
opens entries in the current or a new tab, and supports individual deletion
and clearing all saved history with confirmation. Save browsing history can be
turned off in that view without deleting existing entries.
History stores up to 10,000 recent successful HTTP/HTTPS visits in a local SQLite
database under the Default profile's application-data directory. Private pages,
failed loads, internal pages and local files are excluded; URL usernames and
passwords are removed. Clearing saved history leaves open tabs and their
Back/Forward lists intact. Deployments must include Qt Sql and its QSQLITE driver.
Workspaces keep named, colored tab collections with their own selected tab and
pinned tabs. Open the switcher through the Workspaces menu, the tab overview,
the pill's workspace dot/menu, or Cmd+Shift+E / Ctrl+Shift+E. Next/previous
workspace defaults to Cmd+Option+Right/Left on macOS and Ctrl+Alt+Right/Left
elsewhere. All three shortcuts can be changed in Settings → Keyboard, and
switching remains available when the pill is disabled.
The switcher supports creating, renaming, coloring, reordering and deleting
workspaces. Tabs stay alive while switching; workspace names, order, colors,
tabs, pins and selection are saved locally and restored at startup. Restarting
restores page addresses, not unsaved form contents or per-tab Back/Forward lists.
Pin/unpin tabs with the diamond button in the tab overview; pinned tabs appear
first. Move the active tab using Move tab here in another shared-session workspace.
New workspaces can optionally use separate persistent cookies and website
storage. This choice is fixed at creation; passwords, passkeys and browsing
history remain shared across workspaces. Across different sign-in sessions,
Open copy here opens the address in the destination session and preserves the
original tab. Deleting a workspace requires confirmation and discards its tabs;
website sign-in data is retained. At least one workspace remains available.
Session metadata uses an atomic JSON file and a process lock. Unreadable files
are preserved and concurrent instances cannot overwrite the session.
Cmd+L opens a floating address editor with the full URL selected; Enter navigates, and Escape
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
- CMake 3.24+, Ninja, and a C++20 compiler.
- QtKeychain 0.14.2+ built with Qt 6. CMake prefers an installed package; otherwise
  it downloads and statically builds the pinned 0.14.2 archive after SHA-256
  verification. The initial configure needs network access or a local source override.
- Linux: Qt DBus, pkg-config, libsecret development headers (for example,
  `sudo apt install libsecret-1-dev pkg-config`), and a supported keyring session at runtime.
- On macOS, full Xcode 26+ with Icon Composer and `actool` to compile the layered app icon.

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
sync service. Password secrets are stored separately in the OS credential store;
the local password index contains sites and usernames, but no passwords.
Chromium's sandbox and normal TLS certificate
validation remain enabled.

## Passwords

Press Cmd+Shift+P on macOS / Ctrl+Shift+P elsewhere on an HTTPS login page, then
choose an account to fill. Arbiter never submits a login automatically. The
page context menu exposes the same action, alongside the native Passwords menu
on macOS. Supported form submissions offer
a native save/update prompt. If a website does not trigger one, use **Save current
login** from the password picker. Dismiss the prompt if sign-in failed.

Open **Settings → Manage passwords…** (or **Passwords → Manage Passwords…**) to
search, add, replace, generate, or delete logins. The manager does not reveal
existing saved passwords. Show reveals only a newly entered/generated password.

The first version supports exact HTTPS origins and main-frame forms. It does not
fill embedded frames, hidden fields, ambiguous forms without a focused login
field, or forms posting to another origin. Private profiles do not offer saving.
Usernames can be corrected in the save prompt, including on multi-page sign-ins.
Credentials are local to this profile/device and do not appear in Apple Passwords.

See [password manager architecture and platform validation](docs/password-manager.md)
for storage details, limitations, and runtime checks.

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
The cross-platform workflow is `.github/workflows/password-manager.yml`.
