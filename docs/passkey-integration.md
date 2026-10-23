# Arbiter passkey integration

Engine ownership is approved. The integration lives in the application and the
pinned engine patches under `engine/`; ordinary stock-Qt builds keep local
passkeys and phone controls disabled. The custom runtime builds locally on
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
preserving virtual-authenticator test behavior. The app currently has one
persistent profile, `Default`. Additional profiles must receive separate vault
instances and provider routing before they use this integration.

On Windows the patch retains eligible additional platform discoveries alongside
the OS WebAuthn API and permits explicit Chromium hybrid configuration. It keeps
the RP's original transport filtering and preserves Windows Hello dispatch.
These Windows changes still require native Windows validation.

## Local passkeys

Settings offers vault setup in patched builds. The user chooses a passphrase of
at least 12 Unicode characters and verifies it for each creation/assertion. The
OS credential store alone is not treated as user verification. A confirmation
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
builds WebEngine and excludes the separate, unused QtPdf module:

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
