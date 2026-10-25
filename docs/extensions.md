# Chrome extensions

Arbiter uses Qt WebEngine's native Chromium extension implementation, enabled with
`FEATURE_webengine_extensions=ON` in `engine/build.py`. No JavaScript emulation or
extension code injected into the browser UI is involved. This feature uses the
same implementation on macOS, Windows, and Linux; runtime validation so far is on
macOS.

## Installing and using

1. Focus a view in the workspace/profile that should receive the extension.
2. Open Settings → Extensions. Paste a Chrome Web Store listing URL (or its
   32-character extension ID) and choose **Install from store**. **Browse store…**
   opens the store in Arbiter; supported listing URLs show an Arbiter-owned
   **Add to Arbiter** banner. The website's own **Add to Chrome** button is not
   replaced or emulated. Legacy `chrome.google.com/webstore/detail` links work too.
   Alternatively choose **Install ZIP…** or **Install folder…**. Local ZIPs/folders
   must contain `manifest.json` at their root, with `manifest_version: 3`.
3. A centered permission dialog opens automatically when installation finishes,
   even if Settings is closed. Review the declared API permissions, host permissions,
   and content-script website matches. **Add extension** approves and enables it
   immediately; **Keep disabled** or Escape leaves it disabled. Enabling an existing
   disabled extension uses the same dialog. Long permission lists scroll while
   the action buttons stay visible.
4. Reload existing pages so their content scripts take effect.

The pill's Extensions icon replaces navigation controls with a centered row of
installed extension icons and a back button. Larger collections scroll horizontally
(with wheel or trackpad). Disabled extensions remain dimmed; click an enabled icon
to open its popup, or its settings tab when it only has options. An empty row links
to extension management. The pill restores navigation on Escape, back, or a profile/view
change. Cmd+Shift+X on macOS / Ctrl+Shift+X on Windows/Linux also lists installed extensions. Enabled extensions with action popups
or options pages can be opened here; Settings also has Open/Options buttons.
Keyboard settings can change this shortcut, including with the pill hidden.
Extension action popups open in a compact, non-modal floating panel with rounded
content and a slim bottom drag handle/close button. Clicking the page leaves the
panel open and usable; drag the handle to move it away from page controls. Escape
while the panel has focus, or its close button, dismisses it. Action popups fit
their document's body dimensions and remeasure
while open when content changes; dimensions are capped at 800 CSS pixels and the
available browser window. Extension settings/options open in a regular active tab
in the focused view's workspace and profile. The extension list is non-modal too.

Extension pages use their original Chromium profile, so storage and runtime
messaging work. Popup views are destroyed when the panel closes; navigation,
profile changes, private locks, and shutdown dismiss the panel. External HTTP(S)
navigation opens in Arbiter. Window-opening requests are routed to the focused
workspace. Website-style permission requests inside the extension panel are
rejected; normal web pages retain Arbiter's usual permission UI.

Enable/disable and remove controls are in Settings. Removing is confirmed in the
UI. Reload pages after disabling: injected changes to existing page DOM cannot
necessarily be undone automatically.

## Boundaries

- **Manifest V3 only.** Manifest V2 is rejected by Qt.
- **Store links, ZIPs, and folders.** Store downloads use Google's extension
  update service and support publicly downloadable CRX3 packages. Paid, restricted,
  removed, account-gated, or browser-version-incompatible items can fail with a
  visible error. Local `.crx` import is not exposed. Already installed IDs are
  rejected rather than overwritten. There is no automatic extension updater.
- **A subset of Chrome extension APIs.** Arbiter extends Qt's native bindings with
  the tab, window, scripting, messaging, command, and context-menu integrations
  described below. This is not the complete Chrome API surface. Test each real
  extension rather than assuming installation establishes compatibility.
- Each persistent Chromium profile has its own extension catalog, files,
  approvals, and storage. Workspaces sharing a profile share extensions; isolated
  profiles do not. Split views follow their respective profiles.
- Private profiles do not load user extensions, and their settings show that
  extension installation is unavailable. Qt's built-in PDF and Hangouts
  components are left untouched and excluded from user extension lists.

## Native browser API integration

Upstream-derived API schemas retain Chromium's BSD license; its notice is included
in `licenses/Chromium.txt` and packaged with Arbiter.

These APIs are implemented in the patched engine for extensions generally, with
no extension-ID special cases or injected API shims:

| Area | Supported operations |
| --- | --- |
| Tabs | `query`, `get`, `getCurrent`, `getSelected`, `getAllInWindow`, `create`, `update`, `remove`, `reload`, `goBack`, `goForward`, `getZoom`, `setZoom` |
| Page capture | `tabs.captureVisibleTab`, PNG/JPEG data URLs from the authorized visible page surface |
| Messaging | `tabs.sendMessage`, `tabs.connect`, native content-script routing and `sender.tab` |
| Windows | `get`, `getCurrent`, `getLastFocused`, `getAll`, `update`, `remove` |
| Scripting | `executeScript`, `insertCSS`, `removeCSS`, using Chromium's frame targeting and execution worlds |
| Context menus | `create`, `update`, `remove`, `removeAll`, native click events and checkbox/radio state |
| Commands | `getAll`, manifest keyboard shortcuts and `commands.onCommand` |
| Action metadata | Get/set title, badge text, badge background color, badge text color; `setIcon` with packaged PNG paths |
| Extension status | `extension.isAllowedFileSchemeAccess`, `isAllowedIncognitoAccess` (both false) |
| Cookies | `get`, `getAll`, `getAllCookieStores`, restricted to approved hosts in the calling profile |
| Permissions | `contains`, `getAll`, `request` for access already approved |

Tab creation, updates, activation and removal, and window focus changes emit native
events. `runtime.onInstalled` initializes approved user extensions on their first
activation, with a persisted version marker so disable/enable and restarts do not
repeat installation. A different extension version receives an update reason.
No extension initialization runs merely because its package was installed.
Embedded extension documents use their own extension's first-party storage key,
matching Chromium, so image blobs can be passed to its worker and editor without
relaxing ordinary website storage partitioning.

Tabs have stable IDs, including across sleeping/discarding. The focused
split view is the active tab; extension popups are not browser tabs. Each call
resolves only tabs belonging to the calling extension's profile. Private and
locked views are excluded. URL/title/favicon metadata is scrubbed unless the
extension has the relevant permission.

Opening an action popup, invoking a command, or choosing an extension context-menu
item can grant declared `activeTab` access to that page. This temporary grant is
revoked on an origin change or when the page is discarded/closed. Injection uses
Chromium's permission checks, isolated worlds and resource loader; it does not
execute privileged extension code as ordinary website JavaScript. Capture checks
page permissions again when its asynchronous result returns. Restricted browser,
file, other-extension and store pages are not capture/injection targets. Capture
is limited to two calls per second per extension per tab.

Important limits:

- `permissions.request` never silently expands access. New optional permissions
  return an error until an additional permission-review UI is implemented.
- Action metadata is stored, but badge graphics are not drawn in Arbiter's
  extension list. Actions without `default_popup`, reserved `_execute_action`
  commands and dynamic popup changes are not integrated. `setIcon` accepts packaged
  PNG paths (including size maps); ImageData icons are not supported yet.
- Tab creation supports URL, active/selected, insertion index, window and pinned options. Tab
  updates support URL, activation/highlighting, mute and pin. Unsupported options
  and query filters return errors. Tab movement/grouping and `windows.create`
  are not implemented. Window state/geometry/focus updates are supported.
- Extension context menus use Arbiter's styled, scrollable page menu. Nested items
  are flattened with parent labels. Qt's context-menu request does not identify
  the originating subframe, so click data uses the main-page context (`frameId: 0`).
- Recording APIs (`desktopCapture` and `tabCapture`), native messaging, Chrome
  account sync and automatic extension updates remain unsupported. Permission
  requests from extension popup documents are still rejected.

The bridge uses Qt/Chromium code shared by macOS, Windows and Linux. Runtime
validation of this addition has been performed on macOS; Windows/Linux builds and
real-extension compatibility still need platform testing.

## Cookie permission and consent

Cookie reads require **both** the extension's approved `cookies` API permission
and matching host permission. They can include HttpOnly cookies and sign-in/session
material. The approval dialog displays a prominent warning explaining that access,
in addition to the declared permissions and host list. Existing cookie-capable
extensions approved before this implementation are left disabled until the user
reviews and approves this warning; other extension approvals remain valid.

`activeTab` does not grant cookie access. Store IDs resolve only within the calling
profile, private-profile reads are denied, and asynchronous results recheck that
the extension remains enabled. Cookie writes, change events, and partition-key
queries are not implemented. Unsupported partition queries return an error;
ordinary queries exclude partitioned cookies.

## Investigated compatibility: Awesome Screenshot

Awesome Screen Recorder & Screenshot **4.4.44** (store ID
`nlipoenfbbikpbjkfpfillcgkoblgpmj`) originally installed and opened, but its popup
could not locate a tab: Qt exposed neither `windows.getCurrent` nor `tabs.query`.
Its screenshot workflow also needed capture, content messaging, scripting and
result-tab creation. A disposable-profile probe reproduced those missing APIs on
2026-10-25. This was an engine API limitation, not a missing macOS screen-recording
approval.

The native integration also supplies the extension status queries, cookie reads,
packaged action icons, installation event, and tab insertion index used by this
extension. Embedded extension image blobs share storage with the extension's own
worker, as in Chrome; without that rule the capture toolbar appeared but its
image could not reach the editor.

A disposable-profile test with the unmodified **4.4.44** package verifies visible
page capture, the inline annotation toolbar, and **More annotation tools** opening
`edit-react.html`. It checks the captured fixture's pixels in the editor canvas,
rather than accepting an empty editor. Run the opt-in test with a trusted unpacked
package (no existing profile or credentials are copied):

```sh
ARBITER_TEST_AWESOME_SOURCE=/path/to/unpacked-extension \
  build-patched-tests/extensions_test awesomeScreenshotVisibleCapture
```

Set `ARBITER_TEST_CAPTURE_IMAGE` to save a test-window screenshot. Use Cocoa on
macOS for a rendered capture; an offscreen run does not establish visible-page
capture compatibility. Existing installations requesting cookies must be enabled
again and approved in the permission dialog before using these new APIs.

This verifies the visible-page capture/editor path on macOS, not every feature of
the extension. Selected-area and full-page capture still need separate tests.
Recording remains unsupported, and Qt does not provide `storage.sync`; the
extension can report sync-counter errors even though its captured image reaches
the editor. No Google account synchronization is provided.

References: [Chrome tabs API](https://developer.chrome.com/docs/extensions/reference/api/tabs),
[Awesome Screenshot capture workflow](https://support.awesomescreenshot.com/hc/en-us/articles/53641390758169-How-to-take-capture-a-full-page-screenshot-using-Awesome-Screenshot-extensions).

## Store package verification

`WebStoreInstall` downloads from the store update service without profile cookies
or credentials. Redirects must stay on an explicit Google download host allowlist
and HTTPS; TLS failures are not bypassed. Downloads have a 64 MiB limit, 30-second
network inactivity timeout, 120-second total network deadline, and five-redirect
limit. Progress and cancellation are available while downloading or preparing.
The final Qt copy/install step cannot be cancelled.

Verification and extraction run on a worker thread. `CrxPackage` verifies CRX3
RSA/SHA-256 and P-256 ECDSA/SHA-256 proofs over the specified signed-header context
and complete ZIP payload. Both the developer key matching the requested store ID
and the production store publisher key pinned by Chromium's verifier are required.
The production pin is intentionally not user-configurable; a future Google key
rotation requires a code update. Tampered archives or packages for a different
extension are rejected before extraction.

ZIP validation checks original central-directory and local-header filenames before
Qt's ZIP reader normalizes them. Traversal, absolute paths, symlinks, duplicate
names, encryption, ZIP64/multidisk archives, and unsupported compression are
rejected. There are limits of 10,000 entries, 64 MiB per file, and 512 MiB expanded
size. Directory entries without DOS/Unix attributes (present in real store
packages) are recognized by their original trailing slash. A private temporary
directory is retained until Qt finishes copying the extension, then removed.

Qt uses an unpacked install. The verified developer public key is written to the
manifest's `key` field to preserve the original store ID; the installed ID is
checked again before permission review. No downloaded code runs until the user
enables the extension. Store signatures are verified at download time; this is
not ongoing integrity enforcement over installed files. Locally installed files
and subsequent disk modifications remain subject to the host user's file access.

This uses Qt 6.12's `CorePrivate` ZIP reader (private development headers are a
build dependency) and OpenSSL 3, already used by Arbiter. The private Qt API is
compiled against the selected Qt installation and requires rebuilding when that
Qt installation changes.

Protocol references: [Chromium CRX3 format](https://chromium.googlesource.com/chromium/src/+/main/components/crx_file/crx3.proto),
[Chromium verifier and publisher pin](https://chromium.googlesource.com/chromium/src/+/main/components/crx_file/crx_verifier.cc).

## State and consent

Qt copies installations into the profile's `Extensions` directory and loads them
in a disabled state at startup. `ExtensionCatalog` restores explicitly approved
extensions from `arbiter-extension-consent.json` in that same profile, written
atomically with owner read/write permissions. This file maps each installed ID to
the SHA-256 of the manifest reviewed by the user. A changed or unreadable manifest
requires fresh review. The hash is a consent record, **not** publisher verification
or a signature over an extension's executable files. As with unpacked Chrome
extensions, locally installed files must be trusted.

Disable/remove clears the saved approval. Approval is checked again immediately
before enabling, and only installed user extensions are accepted by mutation
methods. Qt's internal components cannot be removed through this API. Portable
TOML preferences do not copy executable extensions or their approval to another
machine. There is no exported extension backup/restore facility in this release.

## Validation

Build the extension tests in the existing patched test build, then run:

```sh
cmake --build build-patched-tests --target crxpackage_test extensions_test extensionpanel_test --parallel 24
ctest --test-dir build-patched-tests -R '^(crxpackage|extensions|extensionpanel)$' --output-on-failure
```

The test uses synthetic MV3 folders/ZIPs and disposable profile names. It covers
installation and permission review, permission lists, ZIP installation, MV2 and
invalid source rejection, manifest changes during review, persisted enable and
disable state, removal, unknown-ID guards, independent/private profiles, actual
content-script injection, service-worker messaging, shared extension storage,
and an actual action-popup document in Qt Quick WebEngine. The ZIP fixture
contains only a minimal MV3 manifest; no third-party extension code is included.

Qt API reference:
https://doc.qt.io/qt-6/qwebengineextensionmanager.html

`crxpackage_test` uses generated RSA and EC test keys to verify valid packages,
both required proof types, publisher pin/ID mismatches, tampering, truncated and
oversized headers, stable identity, unsafe ZIP names, duplicates, symlinks, and
Manifest V2 rejection. Normal extension tests also cover store link validation
and cancellation. The default suite does not download a store package.

For an opt-in live store smoke test:

```sh
ARBITER_TEST_WEBSTORE=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build-patched-tests/extensions_test liveChromeWebStoreInstall
```

It downloads Dark Reader from the real store, verifies and installs it into a
disposable profile, checks that the store ID is preserved and permission review
is pending, then removes it without enabling it. This validates installation,
not compatibility of Dark Reader's runtime APIs. The live test passed on macOS.

The native API fixture additionally covers profile isolation, stable tab IDs,
query/creation/navigation/pinning, window lookup, messaging sender metadata,
MAIN versus isolated script worlds, file injection and CSS removal, unsafe
resource rejection, action metadata, permission expansion refusal, context-menu
updates/clicks, and scoped `activeTab` grants revoked after cross-origin navigation.

Real compositor capture and actual keyboard command delivery need a visible
native window. On macOS, run the opt-in check against the patched SDK:

```sh
ARBITER_TEST_EXTENSION_CAPTURE=1 QT_QPA_PLATFORM=cocoa \
  DYLD_FRAMEWORK_PATH="$PWD/build-passkey-sdk/lib" \
  build-patched-tests/extensions_test nativeTabsWindowsMessagingAndScripting
```

This decodes the returned PNG and dispatches the manifest shortcut through a real
Qt window. It uses a synthetic extension and disposable profile, not personal
browsing data.

The panel UI regression test clicks and types into a real web page while the
extension remains open, checks that its document is retained, drags its handle,
resizes the window, and closes it using its button. It runs offscreen by default;
`QT_QPA_PLATFORM=cocoa build-patched-tests/extensionpanel/extensionpanel_test`
checks the same interactions with native macOS rendering.

Hidden extension documents can enumerate media devices without a UI delegate.
They have no camera/microphone grant, and capture requests without a permission
UI are denied. `offscreenMediaDeviceEnumeration` covers creation, enumeration and
capture denial and explicit close; it reproduced the opening crash before the engine fix.

To smoke-test a trusted, already unpacked Awesome Screenshot package in a
**disposable profile** (no personal extension storage is copied):

```sh
ARBITER_TEST_AWESOME_SOURCE=/path/to/unpacked/extension \
  QT_QPA_PLATFORM=cocoa DYLD_FRAMEWORK_PATH="$PWD/build-passkey-sdk/lib" \
  build-patched-tests/extensions_test awesomeScreenshotPopupSmoke
```

This enables the supplied extension code and may make its normal network requests.
It checks popup startup and renderer survival, not complete capture/recording
compatibility. No media permissions are granted by the test.

The popup startup smoke test passed on macOS with Awesome Screenshot 4.4.44.
Startup survival does not establish complete extension API compatibility.
