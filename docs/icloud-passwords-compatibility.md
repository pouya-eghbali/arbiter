# Apple iCloud Passwords compatibility

Tested on 2026-10-22 with the installed Qt 6.12.0 macOS kit, Chromium
140.0.7339.225, and Apple's iCloud Passwords extension 3.4.0.

**Result: not functional end to end in this runtime.** Package loading succeeds,
but missing extension APIs and a rejected native-messaging connection prevent
use. These results do not establish the behavior of later Qt releases.

## Package and test isolation

The unmodified Manifest V3 package was downloaded from Google's extension update
service for Apple's [official iCloud Passwords extension](https://chromewebstore.google.com/detail/icloud-passwords/pejdijmoenmkgeppbflobdenhhabjlaj).

- Extension ID: `pejdijmoenmkgeppbflobdenhhabjlaj`
- Version: `3.4.0`
- CRX SHA-256: `610ddd342cab1f66c317da9a24206839829a7ba10376afbb21c94781c3d84f26`

Tests used a separate process and temporary persistent profile. Arbiter's
cookies and credentials were not loaded. No passwords were requested, filled,
exported, or saved. The extension was not installed in Arbiter's normal profile.
No entitlement or system-policy changes were attempted.

## Results

1. Qt's extension manager successfully recognized the package, its official ID,
   and its popup URL.
2. Activation with the legacy profile constructor and subsequently changed
   storage paths crashed in Qt's extension-preference initialization. The stack
   included `QWebEngineExtensionManager::setExtensionEnabled`,
   `ExtensionRegistrar::EnableExtension`, and `PrefService::GetPreferenceValue`.
   Initializing the temporary profile with `QWebEngineProfileBuilder` resolved
   that activation crash.
3. With the profile-builder setup, the extension enabled, but its background
   worker failed with these errors:

   ```text
   Uncaught TypeError: Cannot read properties of undefined (reading 'addListener')
   Service worker registration failed. Status code: 15
   Uncaught (in promise) TypeError: chrome.tabs.query is not a function
   ```

4. The popup page loaded. A connection-only probe in the extension context found
   `chrome.runtime.connectNative` available, but connecting to
   `com.apple.passwordmanager` disconnected with:

   ```text
   Access to the native messaging host was disabled by the system administrator.
   ```

   This is Chromium's reported message, not evidence that a human administrator
   intentionally configured a policy. Source inspection below identifies an
   unimplemented native-messaging delegate consistent with this rejection.
5. A minimal control MV3 extension enabled and opened its popup using the same
   profile-builder setup. Its native-messaging connection received the same
   rejection. Because the control's ID is not in Apple's helper allowlist, this
   does not establish whether the helper would authorize Arbiter.
6. Apple's native helper and system-level Chrome host manifest were present.
   Installing a missing helper therefore is not an established fix.
7. The Apple test process terminated with a trap at shutdown (exit 133), after
   reporting the errors above. The minimal control exited normally.

## Implications

The missing `chrome.tabs.query` API prevents the unmodified background worker
from functioning. Package-load success must not be presented as working password
autofill. Extension API compatibility, native messaging, and Apple's browser
signing and authorization requirements need separate validation before this
can become a product feature.

## Native messaging implementation

The inspected [Qt messaging delegate](https://github.com/qt/qtwebengine/blob/dev/src/core/extensions/messaging_delegate_qt.h)
only overrides `MaybeGetTabInfo`. It does not implement host authorization or
native-host connection creation. Chromium's [base delegate at the tested Chromium version](https://github.com/chromium/chromium/blob/140.0.7339.225/extensions/browser/api/messaging/messaging_delegate.cc)
returns `DISALLOW` from `IsNativeMessagingHostAllowed` and `nullptr` from
`CreateReceiverForNativeApp`. The Qt source inspected was its development branch,
not a verified source match for the installed binary; the runtime rejection is
consistent with those defaults.

This requires engine integration, including native-host discovery, origin
validation, process launch and framed messaging, plus the extension's missing
browser APIs. An Arbiter preference or a user-agent change cannot supply these
implementations.

## Installed Apple helper launch constraints

Read-only inspection of the installed helper's embedded code signature found a
parent launch constraint with alternatives for:

- The `com.apple.developer.web-browser.public-key-credential` entitlement set to
  true; or
- Specific browser signing identifiers paired with their developer Team IDs.

The helper inspected was
`/System/Cryptexes/App/System/Library/CoreServices/PasswordManagerBrowserExtensionHelper.app/Contents/MacOS/PasswordManagerBrowserExtensionHelper`.
The arm64 signature's parent constraint was decoded from its DER representation.
For example, Google's browser identifiers are paired with Team ID `EQHXZ8M8AV`.
Arbiter's signing identity is not among the listed alternatives.

This indicates an additional OS authorization requirement even after Qt's native
messaging support is implemented. The current probe never reached helper launch,
so this is a constraint inspection finding, not an observed helper-launch error.
Approval of the entitlement also does not establish complete extension
compatibility; the full flow still needs testing with a provisioned build.

For Arbiter, the supported next step is an entitlement request through the
organization's Account Holder using Apple's
[macOS browser request form](https://developer.apple.com/contact/request/macos-browsers-passkeys/).
The extension route therefore does not currently remove the need for Apple's
authorization. No helper binary, signature, launch constraint, or browser
identity was modified during inspection.

The temporary probe and downloaded package are outside the repository. No
third-party extension code is bundled with Arbiter.

[Qt extension-manager API reference](https://doc.qt.io/qt-6/qwebengineextensionmanager.html)
