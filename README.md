# Arbiter

A distraction-free desktop browser built with C++, QML, and Qt WebEngine (Chromium).

The project's primary branch is `master`.

The browser has tabs, workspaces, nested split views, an address/search field, back/forward,
reload/stop, loading progress, a persistent local browser profile, and a local
password manager backed by QtKeychain. It starts
on a blank page. Search defaults to DuckDuckGo and is configurable. Favicons
and enabled ad-block filter lists may be fetched in the background. Credential sync is not implemented yet. The required patched-engine integration adds Arbiter
passkeys and Chromium's phone QR transport. The custom runtime builds on Apple
Silicon macOS and passes browser passkey registration/sign-in tests; phone
sign-in on GitHub was also confirmed manually with an iPhone. Windows/Linux
runtime validation remains pending. See
[passkey integration](docs/passkey-integration.md) for build instructions and
current limitations. The current app also requires the patched SDK for
per-view ad blocking.
Standard Qt WebAuthn requests have native account-selection, security-key PIN,
error, and cancellation UI; actual authenticator availability depends on Qt/OS.

Chromium developer tools dock below the focused page: Cmd+Option+I on macOS or
Ctrl+Shift+I on Windows/Linux (customizable in Settings → Keyboard), the pill's
Developer tools item, or right-click → Inspect element. Drag the divider to resize.
See [developer tools](docs/developer-tools.md) for split-view behavior and validation.

First launch offers a short setup: a logo reveal, optional password/history
import via browser cards, Minimal/Glacier/Classic presets, theme plus accent and
background selection, and optional
default-browser setup. It remembers interrupted progress and applies the preset
before opening the browser. Finish with an optional three-step tutorial; replay
it from Settings → Configuration. See [onboarding](docs/onboarding.md) for platform
behavior and verification limits.

The page fills the window. On macOS, the native frame retains rounded corners
and shadows while its title text, background, and window buttons are hidden.
Settings → Appearance → Show native window decorations restores the system title
bar and window buttons. It defaults to off and applies after restarting Arbiter
(`appearance.windowDecorations` in the TOML settings).
The Glacier interface uses blurred, background-tinted glass overlays with soft
shadows and borderless edges. It defaults to
the system color theme. Settings → Appearance offers System, Dark and Light;
the selection is saved and System follows changes to the OS appearance.
Settings uses vertical category tabs for Appearance, Passwords, Passkeys,
Keyboard, Configuration and Import. Checkboxes, dropdown menus and navigation tooltips share rounded,
theme-aware styling.
Appearance saves independent Light and Dark accent and background colors. Choose
a preset or open the centered in-app color picker; Reset restores each color's default.
Background colors tint the browser surfaces, with foreground colors adapting for
readability. The color picker applies changes on confirmation; Cancel preserves
the existing color.
UI icons use a bundled [Lucide](https://lucide.dev/) SVG subset through `UiIcon`,
with consistent sizes and theme colors; full ISC/Feather MIT notices ship with the app.
Buttons, fields and other shared controls live in [`qml/ui`](qml/ui/README.md)
and are reused across Settings, navigation and credential dialogs.
A bottom navigation pill expands on hover or click, then shrinks to a tiny
handle after the pointer leaves. It stays open while using its keyboard controls
or menu. Settings → Appearance → Navigation pill offers a visible handle, an invisible
bottom-edge reveal zone, or shortcut-only navigation;
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
The Glacier passkey prompt presents stacked Arbiter passkey and Use a phone
choices, with a QR flow for phone authentication. The first local passkey
registration creates the vault inside that same prompt and continues the pending
website request; visiting Settings first is unnecessary. The engine capability
bridge is available before provisioning, but credential creation still requires
vault setup and user verification. An empty vault cannot sign in to an account
whose passkey lives elsewhere; choose its provider or Use a phone.

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
The interface follows your OS language from the first onboarding screen, with
English as the fallback. Choose an override in Settings → Configuration → Interface
language, then restart Arbiter. The initial language set includes German, French,
Italian, Spanish, Brazilian Portuguese, Japanese, and Simplified Chinese. All
translations are bundled for offline use. See [translation contributions](docs/translations.md).

History is available from History → Show All History, the navigation pill's
History item, or Cmd+Y on macOS / Ctrl+Y elsewhere. The shortcut can be changed
in Settings → Keyboard. The timeline groups visits by day with visit times and
cached site icons. Combine title/address search with a website filter (including
its subdomains) and Today, Yesterday, Last 7/30 days, or a custom date range.
Custom dates have calendar pickers with month navigation, Today, and Clear;
you can also type YYYY-MM-DD. Ranges are inclusive in your local time zone and
either date may be left blank for an open-ended range. Entries can be opened in the current or a
new tab or deleted individually. The Delete menu separates all saved history
from matching results, with a confirmation showing the scope. Matching deletion
only removes the confirmed visits; visits recorded afterward are kept. Save
history can be turned off in that view without deleting existing entries.
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
Split view supports nested left/right and top/bottom splits in any pane. Start
from the tab overview, the pill's menu, the native Tabs menu, or the configurable
Cmd+Option+S / V shortcuts on macOS (Ctrl+Alt+S / V elsewhere). Splitting opens a
new pane with the start page's address field focused, ready to type a URL or
search. The tab overview can also choose its page. Selecting a visible tab
focuses its pane; another tab replaces the active pane.
Drag the divider to resize, or double-click it to balance the split. Hold
Cmd+Shift on macOS (Ctrl+Shift on Windows/Linux) and drag anywhere in a page
onto another pane to swap them without reloading. Release the modifiers or press
Escape to cancel. Pages fill their panes, with a faint accent edge for the active
pane. Cmd+Option+] / [ (Ctrl+Alt+] / [ elsewhere) cycles pane focus.
Cmd+Option+N (Ctrl+Alt+N elsewhere) replaces the active pane with the next tab
in the focused pane's source workspace that isn't already visible, wrapping around the tab list.
This shortcut is configurable in Settings. The tab
overview and native Tabs menu can keep only the active pane. Closing a tab
collapses its pane. Nested layouts and divider proportions are restored per workspace; if a
layout becomes larger than the window, scrollbars keep every pane accessible.

**Split with…** opens a searchable tab picker grouped by workspace. Use
Cmd+Option+Shift+S on macOS or Ctrl+Alt+Shift+S on Windows/Linux, the tab overview,
or the native Tabs menu. Choose Right, Below, Left, or Above and select a tab;
arrow keys and Enter work from the search field. The shortcut is configurable
in Settings → Keyboard (`KeyboardBindings.splitWith` in TOML).

The current workspace owns the layout, while each pane keeps its tab's original
workspace, cookies, and sign-in session. Borrowing does not move, copy, reload,
or reprofile a tab. A brief workspace badge identifies a focused foreign pane.
The tab overview follows that pane's workspace. Next/previous tab shortcuts and
the next-hidden-tab shortcut cycle only hidden tabs from the focused pane's
source workspace, leaving other panes in place. New tabs and reopening closed
tabs also use the focused pane's source workspace and profile. Switching workspaces restores
each layout, including its focused foreign pane; closing a borrowed tab or
deleting its source workspace removes its references from other layouts.

Cmd+L opens a floating address editor with the full URL selected; Enter navigates, and Escape
or a click outside dismisses the editor. The default empty page has a centered
address/search field, focused on startup. Drag the empty page background to move
the window.

On macOS, Cmd+Left / Right navigates back / forward and Cmd+Up / Down jumps to
the top / bottom of the active page. Windows/Linux use Alt+Left / Right and
Ctrl+Home / End. Native text editing takes priority when a text field has focus.
All four actions are customizable in Settings → Keyboard; existing
saved shortcuts are retained.

The macOS bundle registers HTTP and HTTPS URL schemes. Links opened with
`open -a /path/to/arbiter.app 'https://example.com/'` are delivered to the
existing window, including when the app is launched by the link. Registration
does not make Arbiter the default browser.

Each tab keeps its page and navigation history while you switch between tabs.
Cmd+Shift+A opens a searchable grid of page previews with titles and addresses;
use the arrow keys and Enter to select a tab, click a card, or use its close button. The start page and native Tabs menu
also open the switcher. Websites requesting a new tab open separately, and
external HTTP/HTTPS links open a new tab unless the active tab is empty.
Tabs use their workspace's cookie profile. Reopening a closed tab restores its
latest URL, not its form contents or full navigation history.

Open Settings from the start page or the native Arbiter > Preferences… menu,
with Cmd+, on macOS / Ctrl+, elsewhere, or by
entering `arbiter://settings` in the address editor. Filter the Keyboard page by action name or key combination, then click a binding and press a
new key combination. Bindings save immediately, duplicate assignments are
rejected, and Reset to defaults restores the original bindings.

Start-page and split-view background images can be chosen separately for Light and Dark in **Settings → Appearance**, beside their background colors. Use the Light/Dark editor to customize a mode without changing the active application theme; window and navigation options are grouped below. Images fill the window with a centered crop behind the start-page controls. System theme changes automatically select the matching image. Remove an image to return to the chosen background color; unavailable images also fall back to that color.

Arbiter keeps normalized PNG copies under `backgrounds/` beside `settings.toml`, independent of the original photo. Back up that folder with your dotfiles to include selected pictures. The `appearance.lightBackgroundImage` and `appearance.darkBackgroundImage` settings accept paths relative to the settings directory, absolute local paths, or `file:` URLs. These images stay in the browser UI; website backgrounds are unchanged.

The navigation pill fades/slides into view and smoothly expands from its compact
handle. Main popovers and menus share subtle fade transitions. These transitions
respect the OS Reduce Motion preference; private locking remains immediate.

## Requirements

- Qt **6.12.0 or newer**, including Qt Quick, Quick Controls, **Qt WebEngine**,
  **Qt WebChannel**, **Qt Positioning**, **Qt SVG**, **Qt PrintSupport**, **Qt Pdf**,
  and **Qt Linguist Tools** for compiling the bundled translations.
- CMake 3.24+, Ninja, a C++20 compiler, and Rust 1.93+ with Cargo.
- The current [patched WebEngine SDK](docs/passkey-integration.md#build) for per-view ad blocking and passkeys.
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

The Svelte website lives in [website/](website/README.md). It has its own Node.js
toolchain and static build; it is independent of the browser's CMake build.

Build the current engine patches first using `engine/build.py` (see
[engine build instructions](docs/passkey-integration.md)). Then, for the standard
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

## Website permissions

Website permission prompts and the location switch in Settings → Privacy are
documented in the [site permissions developer guide](docs/site-permissions.md).

## DRM video and codecs

Arbiter does not bundle or download Widevine. When available, it can use a
compatible Widevine module already installed on your device. Widevine is
proprietary Google software governed by applicable Google license terms.
Compatibility with streaming services and playback quality are not guaranteed.

Arbiter's patched WebEngine SDK exposes H.264/AAC formats while relying on
operating-system decoders on macOS/Windows and system-installed FFmpeg on Linux.
It excludes bundled H.264/AAC decoders and the bundled OpenH264 encoder.
H.264 WebRTC encoding/decoding is disabled; WebRTC uses other supported codecs.
Stock Qt builds may omit format support; an application-only rebuild cannot
enable formats missing from the engine. On Linux, compatible FFmpeg development
packages are required at build time and shared libraries at runtime. These
libraries must be installed separately, rather than copied into Arbiter's bundle.
Systems without suitable decoders cannot play those formats; there is no bundled
software fallback. Standalone raw AAC/ADTS playback is not part of the native
decoder integration; the playback probe checks H.264/AAC in DASH/MP4.

Qt searches for an installed CDM at engine startup, including Chrome's standard installation on macOS and
Windows and supported browser/distro locations on Linux. Install Chrome in its
standard location and restart Arbiter before testing protected video. Detection
depends on Qt's search paths; not every browser installation or CPU architecture
is supported. An explicit local library can be selected before launch using
`QTWEBENGINE_CHROMIUM_FLAGS="--widevine-path=/absolute/path/to/library"`.
The library filename is `libwidevinecdm.dylib` on macOS, `widevinecdm.dll` on
Windows, and `libwidevinecdm.so` on Linux. Its architecture and CDM interface must
be compatible with the running engine. CDM discovery does not guarantee that a
particular streaming service will authorize playback or offer HD/4K.

Using OS/system libraries avoids bundling codec implementations; it does not
establish a blanket exemption from applicable patent and software licenses.
Deployment tools must preserve this boundary: do not bundle system FFmpeg or a
Widevine CDM. See
[Qt's codec and DRM documentation](https://doc.qt.io/qt-6/qtwebengine-features.html#html5-drm).

The manual [media probe](tools/media-probe/README.md) checks H.264/AAC support,
Widevine negotiation, and actual encrypted playback using public test media.
See the [developer guide](docs/media-integration.md) for decoder paths, build
flags, packaging checks, troubleshooting, and platform validation status.

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

## Tab sleeping

**Settings → Performance** controls automatic tab sleeping (default 15 minutes;
Off, 1, 5, 15, 30 minutes, and 1 hour). The TOML setting is `Tabs.sleepTimeout`
in seconds, from 0 (off) to 86400. A timeout change wakes sleeping tabs and starts
a fresh interval.

Only hidden, idle tabs that Qt considers safe to pause are eligible. Visible
split panes, the selected tab, audio-playing tabs, loading pages, and pages the
engine recommends keeping active stay awake. Sleep uses Qt's Frozen lifecycle
state to pause background tasks while retaining the document, form contents,
and scroll position. Returning to a tab resumes it without a reload. It does
not discard the renderer or promise to reclaim all its memory. See
[Qt's lifecycle API](https://doc.qt.io/qt-6/qml-qtwebengine-webengineview.html#lifecycleState-prop).

## Passwords

Click a supported username/password field on an HTTPS login page to see a
nonmodal suggestion panel beside it. It lists matching saved logins without
taking keyboard focus. Cmd+Shift+P on macOS /
Ctrl+Shift+P elsewhere opens the centered account picker. Arbiter never submits a login automatically. When available, account selection
uses Touch ID, Windows Hello, or Linux fingerprint verification before reading
the credential by default. Its toggle lives in **Settings → Passwords**, alongside
the cooldown. This verification does not
repeat during the cooldown configured in **Settings → Passwords → Ask me to verify
again** (five minutes by default; Every time is available). The interval starts
at successful verification and is not extended by autofill. Locking, sleeping,
restarting, disabling passwords, or changing the interval clears it. Only the
verification time is remembered, never decrypted passwords. This does not
replace an OS keyring unlock or a legacy macOS Keychain permission prompt; the
system can still require the login password for those. The
page context menu exposes the same action, alongside the native Passwords menu
on macOS. Right-click a supported password field and choose **Suggest password**
to insert a generated password. Explicit new-password confirmation fields in the
same form are filled too; current-password fields are preserved. Generation never
saves or submits a login. Sign-in and account-creation submissions automatically
offer a native save/update confirmation. Dismiss the prompt if sign-in failed.

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

### Portable settings / dotfiles

Preferences and keyboard shortcuts use UTF-8 TOML at
`$XDG_CONFIG_HOME/arbiter/settings.toml`, defaulting to
`~/.config/arbiter/settings.toml` on macOS, Windows and Linux (on Windows, `~`
is your user profile directory). See **Settings → Configuration** for the actual
path and an open-folder button. Copy the file to back up, restore or share it;
restart Arbiter after external edits. A [sample file](docs/settings.example.toml)
shows the available appearance and privacy preferences and shortcut syntax.
Settings made through the UI are written to the same file. The TOML writer
normalizes formatting and key ordering and removes comments; unknown TOML values
are preserved.

Arbiter follows the [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir/latest/)
for configuration: only absolute environment paths are accepted. Shared defaults
are read from `arbiter/settings.toml` under each `XDG_CONFIG_DIRS` entry, in order
(default `/etc/xdg`). Entries use the platform's path-list separator (`:` on
Unix, `;` on Windows). Each key uses the user's value first, then the first
shared file that supplies it, then the built-in default. Shared files are never
modified. Relative `XDG_CONFIG_HOME` values fall back to `~/.config`; relative
`XDG_CONFIG_DIRS` entries are ignored.

On first launch without a user settings file, existing native Qt preferences
are copied into it, preserving the original preferences. An existing dotfile is
always authoritative. `settings.ini` is ignored and is not migrated. Malformed
configuration is preserved instead of being
overwritten. Passwords and passkey secrets remain in their existing protected
storage; history, sessions, browser profiles and caches retain their existing
locations and are not included in this portable preferences file.


### Import from another browser

Open **Settings → Import**, choose a browser/profile, select Passwords and/or
Browsing history, then click **Import from browser**. Import runs only after
this action; there is no startup prompt. Source databases are opened read-only.
The OS can ask you to unlock or authorize access to the selected browser's
credential-store item. Decrypted passwords stay in C++ and are saved through
QtKeychain; no plaintext export file is created by direct import.

On macOS 27+, installed browsers are listed before reading protected profiles.
Select a browser, then use **Allow <browser> access** in Settings (or select its
card in onboarding). The native sheet opens at that browser's exact data folder.
After approval, Arbiter checks that its profiles are readable before proceeding;
confirmation alone does not count as successful access. No credentials are
imported until you start the import.

Profiles are detected for Chrome, Edge, Brave, Chromium, Vivaldi and Arc (macOS),
plus Firefox and Safari history. Direct Chromium-family password import reads
both `Login Data` and `Login Data For Account` when present. macOS uses the
browser's Safe Storage item in Keychain, Linux uses Secret Service or KWallet,
and Windows supports DPAPI-protected local keys and AES-GCM passwords. Source
profiles must belong to the current OS user. Sandboxed distributions may not
have access to another browser's profile or credential store.

Windows app-bound (`v20`) vaults currently require the source browser's own
authenticated export; Arbiter reports that restriction rather than silently
skipping encrypted rows. Firefox and Safari passwords also use the file import
path. Native unlock prompts have not been exercised against personal browser
vaults in automated tests; synthetic encrypted vaults cover decryption and the
end-to-end import flow on macOS. Native Windows/Linux validation remains pending.

**From files** supports password CSV exports from Chrome, Edge, Brave, Firefox,
Safari and Apple Passwords, as well as Safari `History.json`, Chrome Takeout
`BrowserHistory.json`, and Chromium/Firefox/Safari SQLite history databases.
For Safari's browsing-data ZIP, extract it first and choose its password CSV and
history JSON files. See [Apple's export format](https://developer.apple.com/documentation/safariservices/importing-data-exported-from-safari),
[Chrome's password export](https://support.google.com/chrome/answer/95606), and
[Firefox's export instructions](https://support.mozilla.org/en-US/kb/export-login-data-firefox).
Password CSV files are unencrypted; delete the exported copy after importing.

File import shows a preview before saving. Existing passwords are always kept;
duplicate accounts in a source file and unsupported rows are counted and skipped.
Arbiter's existing HTTPS-only login and credential-size limits still apply;
notes, OTP seeds and passkeys are not imported. History keeps original visit
dates, deduplicates URL/time pairs and retains the newest 10,000 visits across
existing and imported history. Import does not turn on paused history recording
or disabled autofill. Stop import finishes the current credential-store write;
completed entries are retained and the result reports partial progress.

### Private browsing

Open a private window from the pill's More menu, the macOS application menu, or **Cmd+Shift+N** on macOS (**Ctrl+Shift+N** on Windows/Linux). The shortcut is customizable in Settings → Keyboard. Each private window has independent, off-the-record cookies, cache, permissions, and site storage. Its tabs, workspaces, and history are never saved or restored, and closing the window destroys that session. Password capture is disabled for private pages; existing saved credentials can still be used. Downloads you explicitly save and passkeys you explicitly create remain on your device. Private browsing does not hide network traffic from websites or network operators, and this visual lock does not encrypt the live renderer's memory.

Set a global private-browsing PIN (at least six digits) or password (at least eight characters) once, in the first private window or Settings → Privacy. A random salt and PBKDF2-SHA256 verifier are saved through QtKeychain in the OS credential store, with insecure fallback disabled; the PIN/password itself is never saved. All private windows share this credential, including after restarting Arbiter, while each window has independent unlock approval. Use Touch ID, Windows Hello (including its PIN), or an enrolled fprintd fingerprint, with the global credential as fallback. Settings → Privacy lets you change it after entering its current value; a successful change locks all open private windows and invalidates pending unlocks. A locked, unreadable, or corrupt credential store never triggers fresh setup. Closing a private window clears its browsing session but retains the global unlock credential.

Private windows heavily blur and darken their contents, and mute audio after five minutes without input, immediately when backgrounded/minimized, and on screen lock/system sleep. Every split pane and popup is protected together. They stay locked after returning or waking; cancelled, failed, or stale authentication cannot reveal them. **Settings → Privacy** configures the inactivity timeout; TOML `[Privacy] lockTimeout` accepts 1–86400 seconds. **Cmd+Option+L** / **Ctrl+Alt+L** locks manually, and is customizable too.


The start page and **Cmd+L / Ctrl+L** share a rounded address prompt with local suggestions: matching open tabs, frequently visited history pages, and a small built-in set of popular sites. Type to filter, use ↑/↓ and Enter to choose, or press Enter without selecting a row to navigate/search your text. Open-tab suggestions switch to the existing tab and its workspace; visible split panes are focused in place. Long lists scroll within the available space. Suggestions do not fetch remote autocomplete or favicons, and private windows use only their own ephemeral tabs/history.

### Address search and favicons

Choose the default search engine in **Settings → Configuration**. DuckDuckGo
remains the default; Google, Bing, Brave Search, Ecosia, and Startpage are also
available. The choice applies to the start page and address shortcut, and is
saved as `Search.engine` in the TOML preferences.

Address suggestions load favicons from Arbiter’s local cache first. Missing
icons are fetched directly from the site’s `/favicon.ico`, without browser
cookies, and cached as small PNGs under Qt’s platform cache directory. Requests
are deduplicated, bounded, and time out; unavailable icons retain the letter
fallback. Private windows use the fallback and make no favicon requests.

### Ad blocking

Built-in blocking uses Brave’s Rust engine with EasyList and EasyPrivacy.
Enable/disable it globally under **Settings → Privacy**. Use the pill shield or
**Command+Option+B** (macOS), **Ctrl+Alt+B** (Windows/Linux) to pause/resume the
focused view. The shortcut is customizable. Rust/Cargo and the current patched
WebEngine SDK are required to build. See [ad-blocking development notes](docs/ad-blocking.md)
for list updates, integration details, tests, and licenses.

### Navigation reveal and sharing

Use **Command+Shift+L** on macOS / **Ctrl+Shift+L** on Windows/Linux to reveal
the pill, even when disabled in Appearance. Escape dismisses the temporary
reveal. The pill's Share button and **Command+Shift+S / Ctrl+Shift+S** open native
sharing for the focused view, with Copy Link included. Both shortcuts are
customizable in Keyboard settings. See [sharing notes](docs/sharing.md) for
platform support, Linux desktop integration, and verification.

### Chrome extensions

Settings → Extensions installs Manifest V3 extensions from Chrome Web Store links, ZIPs, or unpacked folders,
with permission review before enabling. Use the pill’s **Extensions…** menu or
**Cmd+Shift+X** on macOS (**Ctrl+Shift+X** elsewhere) to open extension popups.
The shortcut is customizable. Extensions follow the focused view’s browser profile
and are excluded from private browsing. Store listings show **Add to Arbiter**;
downloads are signature-verified and require permission review before enabling.
Automatic updates and full Chrome API compatibility are not provided yet. See
[extension development notes](docs/extensions.md) for supported behavior and tests.

### PDF viewing

PDF links and local PDF files open in Chromium's built-in viewer inside the tab,
including split panes and private windows. WebEngine's `pluginsEnabled` and
`pdfViewerEnabled` settings are enabled on browser views. The SDK already includes
`webengine_printing_and_pdf`; the separate QtPdf module is not required for viewing.

### Printing

Use **Cmd+P** on macOS / **Ctrl+P** on Windows/Linux, or right-click a page and
choose **Print…**. The shortcut is customizable in Keyboard settings. Printing
uses the focused split pane and opens the system print dialog, with printer,
paper size, orientation, page ranges and copies. Website `window.print()` buttons
use the same flow. The start page and Settings are not printed.
On macOS, the print dialog follows the OS appearance even when Arbiter uses a
different Light/Dark theme.

Native printing requires Qt PrintSupport and Qt Pdf. Chromium prepares the page
with its print stylesheet; the PDF stays in memory and Qt rasterizes it for the
chosen printer at 300 DPI. Rendering/spooling runs off the UI thread. Canceling
the dialog, navigating away, closing the source tab, or locking a private window
cancels preparation; submitted jobs are managed through the OS print queue.

### Downloads

Use the pill’s **Downloads…** menu or **Cmd+Shift+J** on macOS / **Ctrl+J** on
Windows/Linux (customizable in Keyboard settings). Downloads show progress,
pause/resume, cancel, retry for resumable interruptions, Open, and Show in folder.
Compact rows include native file-type icons and icon actions with tooltips and
keyboard labels. Active transfers show smoothed download speed and estimated time
remaining. Estimates reset after pausing/resuming, disappear for unknown sizes,
and return to “Estimating…” when a transfer stalls. File-type icons are resolved
from the OS (or the desktop MIME icon theme on Linux), cached by extension, and
do not inspect downloaded contents.
**Settings → Downloads** chooses the destination; the default is the OS Downloads
folder. Existing files receive numbered alternatives. Completed files never open
automatically. Regular history survives restarts; private lists belong only to
their private window. Clearing the list keeps saved files. See
[download development notes](docs/downloads.md) for lifecycle and platform details.

### Mouse edge navigation

Settings → Appearance → Navigation pill → **Reveal at bottom edge** removes the
mini handle. Pause for 180 ms within the centered bottom strip to reveal the
pill; it stays open over its controls and menus, then collapses after leaving.
The existing shortcut works in all three modes, including **Shortcut only**.

**Swipe back and forward with horizontal overscroll** is enabled by default.
At a horizontal page boundary, start a new two-finger/precision-scroll gesture
and keep swiping until the glass arrow’s ring fills, then release to navigate.
Swipe right for Back and left for Forward with natural scrolling. A short swipe,
reversal, Escape, focus loss, or a dialog cancels. It uses Chromium compositor
ACKs and root overscroll, so content scrolling (including nested scroll areas)
and CSS `overscroll-behavior: contain/none` take priority. Momentum alone and
traditional mouse-wheel scrolling cannot start navigation. It works with input
backends that supply phased scroll gestures; it does not emulate trackpad input
for a conventional wheel. Each split view navigates its own history.

See [the implementation and validation notes](docs/history-swipes.md).

## License

Arbiter is licensed under the **GNU General Public License, version 3 only**
(`GPL-3.0-only`). See [LICENSE](LICENSE) for the full license text.

Third-party libraries, icons, filter lists, and upstream Qt/Chromium files retain
their respective license notices. See [third-party notices](licenses/) and the
[engine license notice](engine/LICENSE).
