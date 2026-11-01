# Arbiter mobile — Qt/QML

Both mobile apps are currently build-your-own: no iOS or Android binary release
is being published.
See [Build your own](#build-your-own) below and the
[release-readiness audit](../docs/mobile-release-readiness.md) for validation limits.

The mobile interface uses Qt Quick and the same Glacier controls, Lucide icons,
filter lists and credential sync format as desktop. Qt WebEngine is selected when
it is available in the Qt kit. iOS uses WKWebView; Android uses Android System
WebView through native platform adapters. Flutter and GeckoView are not required.

This is a development build. It includes a floating pill, tabs, workspaces with
separate sessions, private browsing locks, history, downloads, password save/fill
and generation, native credential providers (iOS passwords/passkeys; Android passkeys),
and encrypted LAN sync.
User-installed extensions and split views are intentionally excluded.

## Build your own

The current mobile distribution plan is **self-built Android and iOS apps only**.
We do not publish prebuilt APKs or IPAs in releases. We are not submitting to Google Play or the App Store yet. No commercial
Qt license is required merely to build the open-source app; comply with the
licenses of Qt and the other dependencies when redistributing your build.

Clone the repository and check out the tag/commit you want to build. Run the
commands below from its root. Use the Qt version pinned in
`engine/manifest.json` (currently 6.12.0), **including its matching desktop host
kit** and Quick, Quick Controls, Quick Dialogs, Quick Effects, Quick Layouts,
SVG, SQL, Network and Multimedia components. Install Python 3.11+, CMake 3.24+,
Ninja and a C++ toolchain. The QR scanner's pinned source is fetched during
configuration. First builds need network access for dependencies.

### Android: build, sign and install your own APK

Android builds need a JDK supported by the generated Gradle/Android plugin
(JDK 17 is the tested choice; newer JDKs must be supported by that Gradle version), Android SDK platform 37, build tools 36.0.0, NDK 27.2.12479018
and Rust/Cargo with the `aarch64-linux-android` target. Point `JAVA_HOME` at your
JDK. The current script targets **ARM64**, not every Android architecture. Keep
Android System WebView updated; workspace isolation and private browsing require
recent multi-profile and data-deletion support.

```sh
rustup target add aarch64-linux-android
python3 tools/mobile/build.py android --release --build-number 3 \
  --qt-kit ~/Qt/6.12.0/android_arm64_v8a --qt-host ~/Qt/6.12.0/macos \
  --sdk "$ANDROID_SDK_ROOT" --ndk "$ANDROID_NDK_ROOT"
```

On Linux/Windows, substitute your matching desktop Qt kit path and Android SDK/NDK
paths. The native builds in this validation pass ran on macOS; Linux/Windows
hosts remain to be tested. Python accepts forward slashes on Windows. These commands are POSIX shell
examples; in PowerShell use `$env:NAME` for environment variables and backticks
for line continuation. `--build-dir PATH` allows a separate build tree.

Create a signing key **outside the repository**, once. `keytool` prompts for the
password interactively; back up the key securely. Keep the same signing key for
future local APK updates, and increment `--build-number` for each build you share.
Do not use the Android debug key for a public release.

```sh
keytool -genkeypair -keystore ~/arbiter-android-release.jks \
  -alias arbiter -keyalg RSA -keysize 3072 -validity 10000
```

Set `ARBITER_ANDROID_STORE_PASSWORD` and `ARBITER_ANDROID_KEY_PASSWORD` in your
local shell or secret manager. Do not place passwords in command arguments or
tracked files. On bash/zsh you can enter them without echoing:

```sh
printf 'Keystore password: '; read -r -s ARBITER_ANDROID_STORE_PASSWORD; printf '\n'
printf 'Key password: '; read -r -s ARBITER_ANDROID_KEY_PASSWORD; printf '\n'
export ARBITER_ANDROID_STORE_PASSWORD ARBITER_ANDROID_KEY_PASSWORD
python3 tools/mobile/package_apk.py \
  --build-dir build-mobile-qt-android --sdk "$ANDROID_SDK_ROOT" \
  --keystore ~/arbiter-android-release.jks --alias arbiter \
  --output output/mobile/Arbiter-android-arm64.apk
unset ARBITER_ANDROID_STORE_PASSWORD ARBITER_ANDROID_KEY_PASSWORD
```

The packaging helper requires a Release build, aligns the APK for 16 KB pages,
signs it, verifies the signature/alignment, and writes a SHA-256 file alongside
it. It does not upload anything. This does not replace testing on a 16 KB device.
Keystores are gitignored, but private signing keys should still live elsewhere.

Install your own signed APK using
`adb install -r output/mobile/Arbiter-android-arm64.apk` after enabling USB debugging
and authorizing your computer. A build signed with a different key cannot update an existing install.
**Uninstalling the old app removes local app data:** sync/export what you need
before doing that. APK signing uses a key you create locally; Google does not issue it. Separately,
Google's Android developer-verification programme can affect installation on
certified devices. As of November 2026, participating-store enforcement has
started in Brazil, Indonesia, Singapore and Thailand, with broader expansion
planned for 2027. Google says ADB installation of local builds does not require
registration. Other installation paths may require verification or the user's
advanced-flow opt-in. See [Google's current guidance](https://developer.android.com/developer-verification/guides)
and [ADB FAQ](https://developer.android.com/developer-verification/guides/faq).
We are not registering an Android developer identity, publishing to a store or
releasing an APK as part of this work.

For debugging, omit `--release`; the generated APK is automatically debug-signed
at `build-mobile-qt-android/android-build/arbiter_mobile.apk`.

### iOS: build and install on your own device

Use macOS with Xcode and the matching Qt iOS device kit; the app supports iOS 18+
and native Liquid Glass requires iOS 26+. The layered app icon requires a recent
Xcode with Icon Composer support. Select Xcode with `xcode-select`, finish its
first-launch setup, and sign in to your Apple development account in Xcode.

The **full app includes a credential-provider extension, App Groups and shared
Keychain access**. Provisioning those capabilities requires an eligible Apple
Developer Program team; a free Personal Team is not sufficient for this documented
full-feature build. This Apple membership is separate from a Qt commercial
license. The simulator does not need signing or a developer membership.

For your own device build, register a unique reverse-DNS application identifier,
its `.CredentialProvider` extension identifier, and an App Group in your Apple
team. Enable the credential-provider capability for the extension and the same
App Group for both identifiers. Keep all identifiers stable to preserve data.
The script generates matching plists and entitlements; do not edit the templates
or commit generated signing files. Example (replace identifiers with your own):

```sh
python3 tools/mobile/build.py ios --release --team YOUR_TEAM_ID \
  --bundle-id com.yourname.arbiter --app-group group.com.yourname.arbiter \
  --qt-kit ~/Qt/6.12.0/ios_device --qt-host ~/Qt/6.12.0/macos
xcrun devicectl list devices
xcrun devicectl device install app --device YOUR_DEVICE_ID \
  build-mobile-qt-ios/Release-iphoneos/arbiter_mobile.app
xcrun devicectl device process launch --device YOUR_DEVICE_ID com.yourname.arbiter
```

Unlock the phone, trust the Mac and enable Developer Mode when requested. Xcode
may require you to register the phone and approve provisioning. Development
signatures/profiles expire and may need renewal. Alternatively open the generated
`build-mobile-qt-ios/ArbiterMobile.xcodeproj` and run the app from Xcode.

The managed default-browser entitlement and unavailable default-browser controls
are **omitted by default** for self-built apps. Do not use `--default-browser-entitlement` unless Apple has approved that
capability for your identifier. This does not prevent normal in-app browsing.
`--device-testing` remains a compatibility alias for the default behavior.

Simulator and desktop preview:

```sh
python3 tools/mobile/build.py ios-simulator \
  --qt-kit ~/Qt/6.12.0/ios_simulator_arm64 --qt-host ~/Qt/6.12.0/macos
python3 tools/mobile/build.py preview --qt-kit ~/Qt/6.12.0/macos
```

Use the x86_64 simulator kit on an Intel Mac. Helpers never install, erase data,
upload or submit to a store without a separate explicit command. Version names
come from `VERSION.txt`; `--build-number` is shared by iOS app, extension and APK.

### Source, privacy and redistribution

The [privacy policy](../docs/privacy-policy.md) is bundled offline in Settings →
Privacy and About, and also has a website page at `/privacy`. Both iOS executables
include privacy manifests. The app manifest merges required-reason declarations
from linked Qt modules; an SDK changing its data-collection declarations stops
configuration for review. Recheck dependency API use and manifests when upgrading
Qt; automatic merging is not a complete legal/privacy audit.

For each redistributed binary, provide its exact corresponding source, Qt source
and patches, license notices and usable build/relink instructions. Our build
scripts and pinned dependencies are part of that source package. A current branch
link alone is not an archive of an old release's source. Android APK distribution
still carries licensing and privacy obligations outside Google Play. App Store
terms, export classification and store-console checks remain future work; see
[the readiness audit](../docs/mobile-release-readiness.md). No license exception
has been added and no commercial Qt license has been purchased.


## Credentials and sync

The C++ client uses the desktop CRDT and encrypted wire format. Pair using a QR
code or invitation, verify the desktop name and certificate fingerprint, then
confirm. Both devices must be on the same LAN, with desktop open and unlocked.
TLS is pinned and payloads are encrypted; there is no hosted relay or cloud sync.
Add multiple computers without replacing existing connections. Edits, conflict
choices and deletion markers propagate through shared peers; see
[credential sync](../docs/credential-sync.md) for pairing steps and device removal.

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
Release-tool/manifest tests: `python3 tools/mobile/test_release_tools.py`.
