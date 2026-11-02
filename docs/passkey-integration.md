# Arbiter passkey integration

Engine ownership is approved. The integration lives in the application and the
pinned engine patches under `engine/`. The current app requires that SDK for
per-view ad blocking as well as passkeys; ordinary stock-Qt test builds can
still cover components that do not require the patched engine. The custom runtime builds locally on
Apple Silicon macOS and passes the browser registration/assertion fixture.
The user confirmed GitHub sign-in using an iPhone through the QR flow on macOS.
Windows/Linux runtime validation and broader phone interoperability remain pending.

## Engine boundary

`engine/manifest.json` pins both the Qt WebEngine repository and its Chromium
submodule. Qt WebEngine **6.140** paired with base Qt **6.12** is intentional:
WebEngine now uses a separate release/version scheme. This is not evidence of a
source mismatch. See [Qt's release announcement](https://www.qt.io/blog/qt-webengine-will-release-separate).

The patch installs a real `FidoDiscoveryBase` / `FidoAuthenticator` for Arbiter.
Chromium continues checking secure contexts, origins, RP IDs, permissions policy,
attachment constraints, and authenticator responses. No page script overrides
`navigator.credentials`, and production never installs a virtual authenticator.

A private, browser-process-only QObject ABI connects the engine to the native
application. `QWebEngineWebAuthUxRequest` carries its request object, so existing
foreground-tab, navigation, Settings, cancellation and timeout handling still
apply. The bridge contains verified origin/RP context, CTAP request bytes, and
public transport information. Private keys remain in the application vault.
The app validates ABI 1 at compilation and through an exported runtime symbol;
a stock engine cannot silently satisfy a patched app build. CMake explicitly
selects all three WebEngine component packages and its helper tools from the
installed SDK, since
the base Qt package otherwise prefers its own sibling modules.
On macOS, compilation also puts the SDK framework directory first so framework
header lookup cannot select the stock WebEngine headers.

The provider is installed only in patched builds. Local credentials are disabled
for off-the-record contexts. The custom provider handles ordinary modal WebAuthn requests; it does not
handle secure payment confirmation or internal browser requests. The patch reports conditional
mediation unavailable until there is a conditional account picker, while
preserving virtual-authenticator test behavior. Workspace profiles can isolate
website cookies/storage while sharing the user's explicitly chosen Arbiter vault.
Only the active foreground tab can present an authentication request; workspace
switching cancels the previous tab's request. Off-the-record profiles remain
excluded. Workspaces do not currently provide separate password/passkey vaults.

On Windows the patch retains eligible additional platform discoveries alongside
the OS WebAuthn API and permits explicit Chromium hybrid configuration. It keeps
the RP's original transport filtering and preserves Windows Hello dispatch.
These Windows changes still require native Windows validation.

## Local passkeys

Desktop passwords and passkeys share the automatically created `CredentialStore`
for the browser profile. It encrypts the credential records with AES-256-GCM and
keeps only a random master key in the OS credential store. No Arbiter passphrase
or separate vault setup is required for new users. Native device verification
is required before creating or using a passkey. macOS prefers Touch ID and offers
the system password; Windows uses Windows Hello. Linux currently requires an
available enrolled fprintd authenticator for this verification. Unavailable or
cancelled verification fails closed; an unlocked sync session cannot authorize a
website challenge.

The existing passphrase format remains readable for migration. Entering the old
passphrase once saves its encryption key into the shared OS-protected store,
retaining credential IDs, signing keys, RP/user binding, and backup eligibility.
Existing persistent biometric enrollment can also supply the legacy key. Source
records remain intact until their authenticated copies are committed. A missing
master key, invalid catalog, or failed decryption never creates a replacement
over existing credentials. The old PBKDF2 passphrase path is retained for legacy
profiles and regression tests, not offered as new-user setup.

OpenSSL generates P-256 keys and DER ECDSA/SHA-256 signatures (COSE ES256). Private
records retain authenticated vault/credential identities. No private material is
published to QML, JavaScript, or the renderer. The provider still validates the
website's RP, user, credential descriptors, ES256 support, and cancellation epoch.
Native verification, focus checks, and navigation cancellation remain separate
from OS keyring access and background sync.

## Device verification and legacy biometric enrollment

Normal passkey operations use fresh native verification: LocalAuthentication's
`deviceOwnerAuthentication` on macOS (Touch ID first, system password fallback),
Windows Hello on Windows, and enrolled fprintd matching on Linux. Foreground and
request-epoch checks reject cancelled, replaced, or background requests. Verification
failures leave the challenge open for retry and never return a signature.

The earlier persistent macOS biometric enrollment remains readable only as a
migration source. Its Data Protection Keychain item uses
`kSecAttrAccessibleWhenUnlockedThisDeviceOnly` and
`kSecAccessControlBiometryCurrentSet`. Successfully unlocking that existing item
can migrate the legacy root without retyping its passphrase. The old per-session
biometric cache is likewise retained only for old-format profiles/tests.

The release packager continues validating any embedded Developer ID provisioning
profile before adding restricted Keychain entitlements. The new shared-store flow
does not require those biometric-item entitlements. Mobile already uses one
OS-protected credential store with native device verification and has no separate
passkey setup passphrase.

## Phone QR transport

The engine generates a fresh 48-byte caBLEv2 QR key for each request, calls
Chromium's `cablev2::qr::Encode`, and configures the discovery factory and network
context. The app encodes that exact `FIDO:/` payload with Nayuki's MIT-licensed QR
encoder and presents it only in native UI. It does not generate a code from a
website URL. The request lifetime owns the QR material, and closing/navigation
clears the rendered image. No persistent phone pairings are stored.

Chromium handles Bluetooth proximity, the encrypted tunnel, and CTAP responses.
The phone keeps its private key. The UI shows the option only when hybrid
transport is available and supports Bluetooth permission/power actions. Both
devices need Bluetooth and internet; OS policies can prevent availability.
The Linux SDK enables Chromium's BlueZ backend. Phone authentication also needs
a functioning system D-Bus, BlueZ service, and Bluetooth adapter. Compiling that
backend out can leave Chromium waiting for discovery even for local passkeys.
Capability probes use the same macOS Bluetooth metadata guard as discovery.
macOS needs `NSBluetoothAlwaysUsageDescription` in the responsible application's
Info.plist; launch the app from Finder when testing. A terminal without that
metadata can cause Chromium to disable Bluetooth discovery.

## Build

Install native Qt WebEngine prerequisites for the target OS, Ninja, Python 3,
and base Qt 6.12.0. See [Qt's platform requirements](https://doc.qt.io/qt-6/qtwebengine-platform-notes.html).
The source checkout and SDK are isolated from the installed Qt kit. The driver
builds WebEngine and excludes the separate, unused QtPdf module.

The same SDK exposes H.264/AAC formats using native macOS/Windows decoders,
with Chromium's FFmpeg configuration that omits their software decoders. Linux builds require system
FFmpeg and fail configuration if it is unavailable. Bundled H.264/AAC decoders
and OpenH264 encoding are excluded; see the README's DRM section and manual
media probe for playback validation. The proprietary-format feature flag
enables format negotiation, not permission to bundle codec implementations.
See the [media developer guide](media-integration.md) for implementation and tests.

```sh
python3 -m venv build-engine-python
# Activate it using the platform's normal venv activation command.
python -m pip install html5lib==1.1
python engine/build.py --qt-prefix /path/to/Qt/6.12.0/platform --jobs 6
```

`--prepare-only` verifies revisions and applies patches without configuring;
`--configure-only` stops before compilation. `--work /path/to/work` relocates the
large checkout, build and SDK. The default is ignored `build-passkey-toolchain/`.
Repeated preparation accepts only the expected patch/overlay and refuses to
replace edited files. The job limit also applies to Qt's nested Ninja process.

On macOS, install the Metal component using
`xcodebuild -downloadComponent MetalToolchain`. If Xcode reports the component
installed but `xcrun metal --version` fails, use the toolchain identifier shown
by `xcodebuild -showComponent metalToolchain` in `TOOLCHAINS` for both configure
and build. The local machine required that explicit selection. The minimum
macOS deployment target is explicitly set to 13.0.
The SDK is built for one architecture, defaulting to the running Python's
architecture; use `--macos-arch arm64` or `--macos-arch x86_64` to select it.
The driver explicitly disables the universal-build flag inherited from the
base Qt kit, so Chromium and Qt use the same architecture when linking.

Then configure Arbiter with the installed custom SDK selected explicitly:

```sh
cmake -S . -B build-passkeys -G Ninja \
  -DCMAKE_PREFIX_PATH="/path/to/work/sdk;/path/to/Qt/6.12.0/platform" \
  -DARBITER_PATCHED_WEBENGINE=ON -DARBITER_WEBENGINE_PREFIX=/path/to/work/sdk \
  -DOPENSSL_ROOT_DIR=/path/to/openssl3
cmake --build build-passkeys --parallel 6
```

For Windows, use an MSVC developer shell and provide OpenSSL 3, for example
through a vcpkg toolchain. Linux needs OpenSSL development headers and the normal
QtKeychain Secret Service/KWallet runtime. macOS can use Homebrew `openssl@3`.
Development builds and browser tests explicitly select the selected SDK's
helper process, resource packs and locales before Chromium starts, as the base
Qt installation would otherwise supply its own paths on Windows/Linux. The
app also adds the SDK's QML module directory to its import paths. Development
builds prepend the SDK library directory to their build RPATH; on Windows,
matching WebEngine DLLs are copied next to the executable before launch. Keep
the custom SDK ahead of the base Qt in any manually set loader environment
variables such as `LD_LIBRARY_PATH`.
The split SDK embeds the SDK and base Qt library directories as runtime
search paths on macOS/Linux, including in the separate WebEngine helper.
On macOS the driver normalizes the installed helper's search order to prefer
the SDK and refreshes its ad-hoc signature after editing the binary.
The SDK therefore depends on that base Qt installation remaining in place.
A distributable app must deploy those files locally, configure its runtime paths and build with
`-DARBITER_WEBENGINE_SDK_RUNTIME=OFF` to remove the development-path overrides.

Ship the custom WebEngine libraries/process/resources/QML module, base Qt dependencies,
OpenSSL dependency and their license notices together; do not mix stock and
patched WebEngine modules. Signing/notarization and release packaging are not
implemented by this change.

`.github/workflows/patched-webengine.yml` defines manually triggered SDK builds
on provisioned macOS/Windows/Linux runners labeled `arbiter-webengine`. Each
runner supplies `ARBITER_QT_PREFIX` and native build prerequisites. The workflow builds/lints the app and runs software tests against each custom
SDK, but has not been run remotely. Updating the engine pin requires reapplying/reviewing
the patches and running the following validation before distribution.

## Validation

`passkeyvault_test` covers encryption authentication, independent signature
verification, registration/assertion, persistence, wrong RP IDs, allow/exclusion
lists, unsupported algorithms, tampered metadata, incorrect passphrases and
failed storage. With `ARBITER_TEST_KEYCHAIN=1`, it also creates and uses a
synthetic encrypted passkey in the real OS store and removes its test entries.
That round trip passes locally on macOS. Windows/Linux are not runtime-validated
on this host; the credential test workflow defines those jobs.

`passkeyengine_test` uses the real browser API to register/assert with an
isolated test vault, verifies the returned signature against the registered
public key and checks rejection of an unrelated RP before native UI. It skips
on stock Qt. Reconfigure the tests against the custom SDK and run this fixture
after the engine completes, passing `-DARBITER_PATCHED_WEBENGINE=ON` and
`-DARBITER_WEBENGINE_PREFIX=/path/to/work/sdk` when configuring `tests/`; a stock
skip does not validate the patched engine.
The complete custom engine builds and installs locally on Apple Silicon macOS.
The final local pipeline on 2026-10-23 completed the patched app build, QML lint,
and all eight CTest entries with `ARBITER_TEST_KEYCHAIN=1` (7.83 seconds for tests).
The real browser fixture passes registration, assertion, signature verification
against the registered public key, and unrelated-RP rejection without a stock-Qt
skip. It uses a temporary named profile because the default private profile
deliberately disables the vault. CTAP response maps use canonical key ordering;
Qt's `SortKeysInMaps` flag did not sort the nested descriptor in the installed kit.
The local pipeline records progress in `build-engine-script-check/status.json`
and compiler output in `build-engine-script-check/engine.log`; after installation
it builds the patched app, lints QML and runs the patched-engine test suite.

Before enabling a release, complete successful local registration/sign-in,
phone scanning/sign-in, wrong-origin/permissions-policy, cancellation/timeout,
Bluetooth denied/off/unavailable, and system/security-key fallback tests on all
three platforms, including Windows native WebAuthn coexistence. On 2026-10-23,
the user reported successful GitHub sign-in using a passkey on an iPhone through
the QR flow on macOS. This is a manual interoperability result; the automated
pipeline still records physical phone validation as `not_run`.

The opt-in signed macOS test `ARBITER_PROTECTED_KEY_PROBE=1 biometricauth_test
protectedKeyRequiresNativeAuthorization` creates a synthetic key, verifies that
Keychain refuses a noninteractive read, then deletes the test item. It requires a
properly provisioned test app and never reads user credentials. Unit tests also
cover persistent unlock after restart/lock, failed enrollment, cancellation,
changed keys, migration, disabling, and passphrase fallback.
