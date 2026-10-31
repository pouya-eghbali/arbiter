# Arbiter mobile — Qt/QML

The mobile interface uses Qt Quick and the same Glacier controls, Lucide icons,
filter lists and credential sync format as desktop. Qt WebEngine is selected when
it is available in the Qt kit. iOS uses WKWebView; Android uses Android System
WebView through native platform adapters. Flutter and GeckoView are not required.

This is a development build. It includes a floating pill, tabs, workspaces with
separate sessions, private browsing locks, history, downloads, password save/fill
and generation, a native password/passkey provider, and encrypted LAN sync.
User-installed extensions and split views are intentionally excluded.

## Build

Use the Qt version pinned in `cmake/Dependencies.cmake` (currently 6.12), including
Qt Quick, Quick Controls, Quick Dialogs and Multimedia. The QR scanner uses
zxing-cpp 3.1.1. Android additionally requires Rust's `aarch64-linux-android`
target, JDK 17, Android SDK 37 and NDK 27.2. iOS requires Xcode and iOS 18 or later.

Run from the repository root (replace kit paths with your installation):

```sh
python3 tools/mobile/build.py preview --qt-kit ~/Qt/6.12.0/macos
python3 tools/mobile/build.py ios-simulator \
  --qt-kit ~/Qt/6.12.0/ios_simulator_arm64 --qt-host ~/Qt/6.12.0/macos
python3 tools/mobile/build.py ios --release --device-testing --team YOUR_TEAM \
  --qt-kit ~/Qt/6.12.0/ios_device --qt-host ~/Qt/6.12.0/macos
python3 tools/mobile/build.py android \
  --qt-kit ~/Qt/6.12.0/android_arm64_v8a --qt-host ~/Qt/6.12.0/macos \
  --sdk "$ANDROID_SDK_ROOT" --ndk "$ANDROID_NDK_ROOT"
```

The helper does not install, upload or wipe anything. Output lives in
`build-mobile-qt-<platform>`. Android produces `android-build/arbiter_mobile.apk`.
iOS produces `<configuration>-iphoneos/arbiter_mobile.app` and embeds the native
credential-provider extension. Install with `adb install -r`, `simctl install`,
or `devicectl device install app` as appropriate.

Apple's managed default-browser entitlement requires approval. `--device-testing`
omits it for local provisioning; production builds retain it. App-group and
keychain entitlements are required for the app and credential provider. Provision
both bundle identifiers with the same team. No signing secrets are stored here.

## Credentials and sync

The C++ client uses the desktop CRDT and encrypted wire format. Pair using a QR
code or invitation, verify the desktop name and certificate fingerprint, then
confirm. Both devices must be on the same LAN, with desktop open and unlocked.
TLS is pinned and payloads are encrypted; there is no hosted relay or cloud sync.

Credentials are AES-GCM encrypted. iOS keeps the wrapping key in the shared
Keychain access group; Android uses Android Keystore to wrap the random vault key.
Native credential providers perform their own authorization and RP checks.
Existing prototype credential files retain their format and native key migration.
The remaining mobile UI translation drafts use English fallbacks where a
translation is unavailable. Authentication uses biometrics with the device passcode as fallback. Background
refresh is opportunistic and subject to OS scheduling and lock state.

Android's WebView has no public off-the-record profile API. Private tabs use a
separate, temporary profile with cookies and persistent DOM storage disabled;
its site data is cleared after the last view closes. Android can retain the
loaded profile shell until process exit; stale private profiles are deleted at
next startup before any new views are created. This is more restrictive than
regular browsing, but does not promise identical storage behavior to Chrome's
incognito mode. iOS uses nonpersistent WKWebsiteDataStore instances.

## Blocking and native integration

Android and WebEngine invoke our existing Rust adblock engine for requests. iOS
loads native WebKit content-blocking rules generated from the same filter lists;
WebKit cannot express every filter feature. No Gecko extension or downloaded
filter engine is used. WebView itself controls web compatibility and updates.

Qt/QML sheets use an in-memory page backdrop while open. Native adapters expose
only the rounded floating-pill region to Qt Quick and route its touches correctly,
so the page continues behind the pill at full viewport height. Snapshots also
supply the glass effect and tab thumbnails; private previews clear on lock and
never enter saved sessions. Camera scanning uses Qt Multimedia and native OS
permission UI. See [the interface guide](../docs/mobile-interface.md).

## Tests

```sh
ctest --test-dir build-mobile-qt-preview --output-on-failure
ARBITER_TEST_DESKTOP_FIXTURE="$PWD/build-release-tests-fcd301a1ff60/credentialsync/credentialpeer_fixture" \
  QT_QPA_PLATFORM=offscreen build-mobile-qt-preview/mobile_core_test
```

The optional peer test uses temporary files and dummy credentials, never your
real vault. Android instrumentation tests in `platform/android/tests` check
passkey crypto, the native scheduled-sync service and actual WebView/profile
isolation. Run the browser UI test on a disposable emulator; it deliberately
skips physical devices to protect personal browser profiles. From the generated
`android-build` folder, run `./gradlew connectedDebugAndroidTest`. Test real WebView behavior
on both operating systems before publishing a mobile release.

Native Swift crypto checks: `python3 tools/mobile/test_native_crypto.py`.
