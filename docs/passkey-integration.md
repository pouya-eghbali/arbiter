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

Settings offers vault setup in patched builds. The user chooses a passphrase of
at least 12 Unicode characters. Each creation/assertion verifies that passphrase,
or uses fresh device verification with an optional, short-lived biometric session.
The OS credential store alone is not treated as user verification. A confirmation
click alone never sets the UV flag.

- OpenSSL 3 generates P-256 keys and produces DER-encoded ECDSA/SHA-256 signatures
  (COSE ES256, algorithm -7). No custom cryptographic primitives are implemented.
- A random 32-byte salt and PBKDF2-HMAC-SHA-256 with 600,000 iterations derive the
  vault encryption key. AES-256-GCM protects the verification record and each
  private credential, with fresh random nonces and vault/credential identities
  bound as authenticated context. Verification failures have an in-process
  exponential backoff. The work factor also applies to offline guessing.
- QtKeychain stores the protected verification record and private credentials
  under the `Arbiter Passkeys` service.
  Plaintext fallback is disabled. Each namespaced item is at most 2,560 bytes,
  matching the Windows credential-store backend's limit.
- An owner-only atomic CBOR catalog under `passkeys/Default` contains public
  credential IDs, RP IDs, user handles and account names. It is not encrypted.
  The encrypted record repeats this metadata; modified catalog entries cannot
  redirect a private key to another RP/account. A lock prevents concurrent
  writers. Identities are reserved before OS-store writes; interrupted setup
  cannot silently replace an existing vault.
- Credentials are discoverable, device-bound, with backup flags clear, zero
  AAGUID, `none` attestation and a constant zero signature counter. There is no
  claim of hardware protection, synchronization, enterprise attestation, PRF,
  large-blob support or credential management protocol support.
- Allow/exclusion lists, user-handle bounds and ES256 negotiation are enforced.
  Accounts are selected by credential ID, including accounts with equal names.

There is currently no passphrase change/recovery, sync, export/import or passkey
removal UI. Losing the profile catalog, system-store entries or vault passphrase
can make local passkeys unavailable. The setup UI explains this. Sites that
require other algorithms or hardware/enterprise attestation need another
authenticator. Interrupted registration can leave an unused local credential.

The user-verification and authenticator-data model follows
[WebAuthn](https://www.w3.org/TR/webauthn-3/); the PBKDF2 work factor follows
[OWASP's guidance](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).

## Biometric session unlocking

This is optional convenience unlocking for **Arbiter's own passkeys**; it does not
provide access to Apple Passwords or replace the phone QR flow. Enable it in
Settings → Passkeys using the vault passphrase and a fresh device verification.
Existing vaults keep their encrypted header/private records and gain only an
optional boolean in the public catalog; no vault migration/re-encryption is needed.

- macOS uses a new `LAContext` for each attempt, the biometric-only policy and
  zero Touch ID reuse. [Apple's LocalAuthentication API](https://developer.apple.com/documentation/LocalAuthentication/LAContext/evaluatePolicy%28_%3AlocalizedReason%3Areply%3A%29)
  performs verification; there is no Apple browser entitlement or Passwords access.
- Windows uses HWND-owned `IUserConsentVerifierInterop` and accepts only
  `UserConsentVerificationResult::Verified`. This is Windows Hello verification,
  which can include a Hello PIN as well as biometrics. The desktop interop API
  requires Windows 11 / build 22000 or newer; older/unconfigured systems retain
  the passphrase path. Builds use MSVC/C++/WinRT, a Windows 11 SDK, Qt Concurrent
  and `windowsapp`. See [Microsoft's desktop API](https://learn.microsoft.com/en-us/windows/win32/api/userconsentverifierinterop/nf-userconsentverifierinterop-iuserconsentverifierinterop-requestverificationforwindowasync).
- Linux uses the system D-Bus fprintd service for the current caller: discover a
  reader, check enrollment, claim it, start verification, accept only `verify-match`,
  then stop/release it. Attempts have separate signal receivers/tokens, and reader
  cleanup finishes before another attempt starts. Configure a compatible reader,
  enroll fingerprints and use the desktop's normal fprintd/PolicyKit policy.
  Arbiter does not install or change system authentication policies. See the
  [fprintd device API](https://fprint.freedesktop.org/fprintd-dev/Device.html).

Only the passphrase-derived key is cached, in a dedicated locked memory page
(`mlock`/`VirtualLock`, with dump exclusion where supported). Failure to lock the
page prevents biometric enrollment; passphrase operations continue normally.
Neither the vault passphrase nor a biometric unlock key is written to the catalog,
QtKeychain, files or credential-provider APIs. Existing encrypted credentials stay
protected by the passphrase. This is an in-process session, **not** a hardware-bound
persistent alternative key or protection against a compromised browser process.

The session expires 15 minutes after successful passphrase verification and is
wiped when Arbiter becomes inactive, on screen-lock/sleep notifications, on shutdown, on loss of availability or with
Lock now. A native verification dialog may temporarily take focus; authorization
is delivered only when Arbiter is active again. Every operation still requires a
new native verification, and re-checks the encrypted vault verifier with the cached
key before using it. Returning to Arbiter after locking/leaving/restarting requires
the passphrase to warm the session again. Disabling the option clears the cache.

Cancellation, timeout, navigation, workspace changes and replaced WebAuthn requests
invalidate in-flight results. Failed/dismissed biometric prompts keep the current
WebAuthn challenge open for the passphrase fallback. Verification never generates
WebAuthn output after cancellation. Headless tests inject a verifier and in-memory
secret store; the production backend never uses those implementations.

The macOS app builds and passes simulated-verifier browser enrollment,
registration/assertion and fallback tests. Native capability detection and the
Linux backend's disconnected-D-Bus path have been exercised locally. Real Touch ID,
Windows Hello and Linux fingerprint matching still require hardware validation;
Windows builds/runtime validation remain pending. The existing desktop CI matrix
also compiles/probes the native platform backend without prompting for biometrics.

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
