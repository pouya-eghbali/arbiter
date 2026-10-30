# Arbiter mobile

A shared Flutter interface for Android and iOS, with the floating pill, tab previews,
workspaces, private browsing, history, downloads, and an encrypted credential vault.
Android renders pages with GeckoView; iOS uses the system WKWebView. Desktop continues
using the patched Qt WebEngine runtime.

This is a development build, not a published mobile release. It includes built-in
ad blocking, password suggestions/generation and save prompts, automatic tab sleeping,
onboarding, and offline UI translations. The interface defaults to the OS language,
with English, German, French, Italian, Spanish, Portuguese, Japanese and Chinese.
Translations reuse desktop catalogs and locally drafted mobile strings; native-speaker
review remains worthwhile. User-installed extensions and split views are excluded.
Desktop settings do not automatically transfer to mobile.

## Build

Use Flutter **3.47.8** / Dart **3.13.7** (`.fvmrc`), Xcode 27.1 for the tested iOS
simulator, or JDK 17 and Android SDK 37.2. Android requires API 26 or newer;
system passkey provider integration requires API 34. iOS requires version 17 or newer.
The Android build pins GeckoView 159.0.20261022150525, AGP 9.5.0 and Gradle 9.6.0.

From this directory, with that Flutter SDK on PATH:

```sh
flutter pub get
flutter analyze
flutter test
flutter build apk --debug
flutter build ios --simulator --debug -t lib/main.dart
```

Debug APK: `build/app/outputs/flutter-apk/app-debug.apk`.
iOS simulator application: `build/ios/iphonesimulator/Runner.app`.
These are test artifacts. Android release signing deliberately has no debug-key
fallback. Configure private release signing before distributing an APK/AAB.
Open `ios/Runner.xcworkspace` for physical-device provisioning and archives.

### Apple capabilities

Register the app and credential extension with these identifiers (or change all
identifiers and the shared configuration together):

- App: `com.arbiter.mobile`
- Credential extension: `com.arbiter.mobile.CredentialProvider`
- App Group: `group.com.arbiter.mobile`
- Keychain access group: `$(AppIdentifierPrefix)com.arbiter.mobile.credentials`

Enable App Groups and the shared Keychain group for both targets. Enable AutoFill
Credential Provider for the extension. Request Apple's **managed default-browser
entitlement** for the main app; it is declared in `Runner.entitlements`, but Apple
approval and a matching provisioning profile are still required. Without that
approval, arbitrary website passkeys in WKWebView and default-browser eligibility
must not be represented as ready for distribution. The app uses system WebKit,
not an alternative iOS engine.

[Apple browser requirements and request form](https://developer.apple.com/documentation/xcode/preparing-your-app-to-be-the-default-browser)

For a local physical-device test before Apple grants that capability, first run
`flutter build ios --release --no-codesign -t lib/main.dart`, then sign with:

```sh
xcodebuild -workspace ios/Runner.xcworkspace -scheme Runner \
  -configuration Release -destination 'id=YOUR_IPHONE_UDID' \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
  DEVELOPMENT_TEAM=YOUR_TEAM CODE_SIGN_STYLE=Automatic \
  ARBITER_BROWSER_ENTITLEMENTS=Runner/DeviceTesting.entitlements \
  SYMROOT="$PWD/build/ios" build
xcrun devicectl device install app --device YOUR_IPHONE_UDID \
  build/ios/Release-iphoneos/Runner.app
```

Run those commands from `mobile/`. The test entitlements retain shared Keychain,
App Groups and the credential extension, but omit default-browser eligibility.
This build cannot validate arbitrary website passkeys inside WKWebView. The
normal build still uses `Runner.entitlements` and requires Apple's approval.

### Android system passkeys

Set Arbiter as the default browser, then enable its credential provider from
Passwords & passkeys. Android verifies a browser's signing identity before allowing
it to supply a website origin to a credential provider. The provider bundles Google's
public privileged-browser certificate list and recognizes its own installed signer.
It currently accepts browser WebAuthn requests with a validated origin and client-data
hash, not arbitrary native-app requests requiring Digital Asset Links. There is no
Android system-wide password AutofillService yet; saved passwords can be filled from
Arbiter's vault into the current matching HTTPS page.

## Local encrypted sync

Build the desktop app from the same revision. In desktop Settings → Sync, unlock
the vault and show its pairing QR. On the phone open Passwords & passkeys, unlock,
and scan the QR (or paste the invitation). Verify the desktop name and certificate
fingerprint in the confirmation. Keep the desktop app open on the same local network.
The desktop vault unlock normally expires after ten minutes. For background sync,
explicitly enable “Keep sync unlocked until this computer locks” in desktop Sync
settings. Computer lock, manual sync lock and quitting Arbiter still clear its keys.
The phone syncs every twenty seconds while foregrounded, including while its
credential list is locked. Background sync is separately configurable. Android
uses an OS-scheduled unmetered-network job; iOS uses background refresh. Timing is
chosen by the OS, not guaranteed. The phone's OS vault must be accessible (unlocked
on iOS), and the paired desktop must be reachable on the same LAN. Opening credentials or making a passkey assertion still
requires device authentication.

New Arbiter passkeys support sync. Older desktop passkeys registered as device-bound
are excluded; register a replacement to make one eligible for sync.

No cloud account, relay, or remote server is used. Guest Wi-Fi client isolation,
VPN routing, firewalls and changed desktop addresses can prevent connectivity.
Pair again after an address change. Invitations expire after two minutes and are
single-use. Revoke a lost phone from desktop Sync settings. Disconnecting on the
phone preserves its local credentials and does not revoke its desktop token.

The protocol and trust boundary are documented in [credential-sync.md](../docs/credential-sync.md).

## Browser features

The shield in the pill pauses blocking for the current website; Settings controls
it globally. Android bundles the standalone Ghostery filter engine in a first-party
GeckoView extension. iOS compiles WebKit content rules generated with Brave's Rust
converter. Both use the repository's EasyList + EasyPrivacy snapshots. Lists are
bundled and refreshed through app builds, not downloaded at runtime. Some ABP rules
cannot be represented by WebKit. Blocking is tested against real network requests;
it cannot guarantee every ad or tracker is blocked.

HTTPS login fields offer a compact password strip. Selecting a saved account requires
device authentication; its reuse interval is configurable (including every time).
Suggestions and save offers can be disabled independently. Private tabs never offer
to save logins. Submitted credentials are held briefly in memory and offered for
saving after the password form disappears; ordinary submissions and common SPA
submit-button flows are handled. This is heuristic and may miss unusual login forms.
Generation uses cryptographically secure randomness, 16–64 characters and optional
symbols. Native filling is restricted to the same top-level origin.

Inactive tabs sleep after a configurable interval. The focused tab, pages playing
media, loading pages and edited top-level forms stay awake. Sleeping releases the
native page and reloads its current URL on selection; in-page state and the native
back/forward stack are not retained across sleep.

## Test

Unit/widget tests run without a browser engine. Native tests need an emulator or
simulator; `flutter devices` lists their IDs:

```sh
flutter test integration_test/browser_engine_test.dart -d DEVICE_ID
flutter test integration_test/credential_keychain_test.dart -d IOS_SIMULATOR_ID
flutter test integration_test/adblock_test.dart -d DEVICE_ID
flutter test integration_test/form_events_test.dart -d DEVICE_ID
flutter test integration_test/background_runtime_test.dart -d IOS_SIMULATOR_ID
```

The form test exercises browser-generated submissions, rejects synthetic events,
and checks that disabling password saving survives navigation.
The native engine test covers navigation, snapshots, origin-bound password fill,
private storage and isolated workspace storage. The Keychain test uses and removes
a unique dummy item in the shared iOS group.

Android cryptography and actual headless JobScheduler instrumentation (JDK 17):
first build the normal app with `flutter build apk --debug -t lib/main.dart`. The
headless worker smoke test never opens a user vault.

```sh
cd android
./gradlew :app:connectedDebugAndroidTest
```

For iOS native crypto tests, first run the normal `flutter build ios --simulator
--debug -t lib/main.dart` command. Flutter integration tests temporarily select a
generated Dart entrypoint; a normal build resets it before direct Xcode testing:

```sh
cd ios
xcodebuild -workspace Runner.xcworkspace -scheme Runner -configuration Debug \
  -destination 'platform=iOS Simulator,id=DEVICE_ID' test
```

For a real TLS desktop/phone exchange, build `credentialpeer_fixture` in the desktop
test build, then from the repository root:

```sh
python3 tools/mobile/test_peer.py --device DEVICE_ID \
  --fixture DESKTOP_TEST_BUILD/credentialsync/credentialpeer_fixture \
  --flutter /absolute/path/to/flutter/bin/flutter
```

The fixture uses temporary files, in-memory secrets and fabricated credentials. It
never reads the user's vault. The phone test checks pinning, pairing and a merge
against the native desktop implementation. OS picker UI, biometrics and real website
registration/assertion still need physical-device acceptance testing before release.

## Credits

Arbiter is GPL v3. Flutter/Dart and plugins retain their own licenses, listed in
Settings → Open-source licenses. GeckoView is an unmodified Mozilla distribution;
its bundled license notices and source link are included there. Lucide icons are
ISC, copied from desktop's pinned revision `7faf876` (fingerprint-pattern and trash
have compatibility filenames fingerprint and trash-2). The browser certificate list
is public application/signing metadata from
<https://www.gstatic.com/gpm-passkeys-privileged-apps/apps.json>, retrieved 2026-10-30;
there is no runtime request to that endpoint. Keep it current when updating engines.

See [tools/mobile/README.md](../tools/mobile/README.md) for reproducible blocker
assets, native localization resources, and common isolated form-script maintenance.
