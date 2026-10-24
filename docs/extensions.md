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

The pill's More menu → Extensions, or Cmd+Shift+X on macOS / Ctrl+Shift+X on
Windows/Linux, lists installed extensions. Enabled extensions with action popups
or options pages can be opened here; Settings also has Open/Options buttons.
Keyboard settings can change this shortcut, including with the pill hidden.
Extension pages use their original Chromium profile, so storage and runtime
messaging work. Their views are destroyed when the panel closes; navigation,
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
- **A subset of Chrome extension APIs.** Qt does not provide Chrome's complete
  tabs/windows/toolbar integration. In particular, `chrome.tabs.query` and
  active-tab-dependent workflows are not supplied by this initial integration.
  Toolbar actions without `default_popup` cannot be triggered through Arbiter's
  panel. Extensions can still operate as background/content-script extensions.
  Native messaging, Chrome account sync, and extension shortcut commands have
  not been integrated. Test each real extension rather than assuming that a
  successful installation establishes compatibility.
- Each persistent Chromium profile has its own extension catalog, files,
  approvals, and storage. Workspaces sharing a profile share extensions; isolated
  profiles do not. Split views follow their respective profiles.
- Private profiles do not load user extensions, and their settings show that
  extension installation is unavailable. Qt's built-in PDF and Hangouts
  components are left untouched and excluded from user extension lists.

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

Build `crxpackage_test` and `extensions_test` in the existing patched test build, then run:

```sh
cmake --build build-patched-tests --target crxpackage_test extensions_test --parallel 24
ctest --test-dir build-patched-tests -R '^(crxpackage|extensions)$' --output-on-failure
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
