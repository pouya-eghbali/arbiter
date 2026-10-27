# Desktop releases and dependency updates

The main release path is **build locally, then upload to a GitHub draft**. No
self-hosted runner or startup service is required. GitHub handles the small
source-draft and dependency-check jobs; Chromium compilation happens on our Mac,
Windows machine, and Linux Docker container. Nothing publishes a public release automatically.
This is release engineering, not an in-app auto-updater.

## Platform entry points

`/env.sh` is ignored by Git. The macOS script loads it automatically with export
and command tracing disabled. Never add signing secrets to tracked files.
These scripts build/test/package only; uploading is a separate explicit command.
Commit source changes before building, then use the same release tag on each machine.

### macOS: build, sign, notarize

```sh
# Base Qt and Python/Rust prerequisites below still apply.
export ARBITER_QT_PREFIX=/absolute/path/to/Qt/6.12.0/macos
./tools/release/build-macos.sh --jobs 24
# Optional: --sdk-prefix /absolute/path/to/a/verified/sdk
```

The script accepts the names in the local `env.sh`: `APPLE_DEVELOPER_ID`
(Developer ID Application signing identity), `APPLE_ID`, `APPLE_APP_PASSWORD`
(app-specific password), and `APPLE_TEAM_ID`. It validates/stores notarization
credentials in the login Keychain profile `arbiter-release`, without logging
credential values. Alternatively set `ARBITER_MACOS_SIGNING_IDENTITY` and an
existing `ARBITER_NOTARY_PROFILE`; no Apple password variables are then needed.
The Developer ID certificate/private key must already be installed in Keychain.
The build is followed by nested signing, notarization, stapling, and ZIP creation.
This entry point requires signing; use the Python driver directly for unsigned tests.

### Windows: native build

From an x64 MSVC Developer PowerShell, with the pinned Qt, Python/Rust and OpenSSL
installed and the variables documented below configured:

```powershell
./tools/release/build-windows.ps1 -Jobs 24
# For testing without a signing certificate:
./tools/release/build-windows.ps1 -Jobs 24 -Unsigned
```

`-SdkPrefix` reuses a verified SDK. `ARBITER_PYTHON` can select the Python executable;
otherwise the scripts prefer `build-release-python` and then system Python.

### Linux: Docker build

Run on the Windows x64 machine with Docker Desktop using Linux containers:

```powershell
./tools/release/build-linux.ps1 -Jobs 24
```

Or on a Linux x64 Docker host:

```sh
./tools/release/build-linux.sh --jobs 24
```

The image provisions Ubuntu 24.04, Node 22, Qt from the engine manifest, pinned
Python/Rust and build dependencies. QtPdf is built from the pinned WebEngine
source against the base kit because it follows the separate WebEngine version
series. Image creation includes this one-time compilation. No host Qt installation is required. The container builds
a committed-source Git bundle, runs tests as a non-root user with the Chromium
sandbox enabled, then copies the Linux archive/checksum to
`build-release-artifacts/`. It never mounts `env.sh`, the host checkout, or Apple
credentials. A named Docker volume caches source/SDK builds. Allocate substantial
RAM/disk to Docker and reduce jobs if needed. Docker's host kernel must permit
Chromium sandbox user namespaces. The bundled Moby-derived seccomp profile
permits namespace creation inside the non-root build container so Chromium can
install its own sandbox; other Docker default restrictions remain. Apple Silicon Docker engines require explicit opt-in;
the Windows x64 machine is preferred. To explicitly allow slower emulation on
an ARM Docker engine, run `./tools/release/build-linux.sh --allow-emulation --jobs 12`.
`ARBITER_DOCKER_UID`/`ARBITER_DOCKER_GID` optionally select the non-root container user.
The image creates a matching Linux account entry, which D-Bus and Secret Service
require even when Docker can already run processes under the numeric UID.
If the exact Qt pin is not yet available from the public Qt archive, set
`ARBITER_LINUX_QT_PREFIX` to a pre-provisioned Linux x64 kit (including QtPdf) on
the host. It is mounted read-only and skips image Qt provisioning. This must be
a Linux kit, even when the Docker host is Windows.

### Upload from either machine

Install GitHub CLI and run `gh auth login`. Check out the matching local release
tag, which must already exist on GitHub. Each machine can upload independently:

```sh
./tools/release/upload.sh --tag v0.1.0 --platform macos
./tools/release/upload.sh --tag v0.1.0 --platform linux
```

```powershell
./tools/release/upload.ps1 -Tag v0.1.0 -Platform windows
# Add -Unsigned for unsigned testing builds.
```

Use `--unsigned` in Bash for unsigned testing packages. `--assets` / `-Assets`
selects another package directory. Omit platform selection to require/upload all
three packages together. Version, exact commit, engine identity, dependency pins,
signing status and checksums are verified for each selected package. Partial uploads
preserve existing draft notes and other platforms, and add per-platform checksum
manifests/sidecars. Existing packages of the same filename are replaced. Nothing
publishes automatically; published releases are refused. Review all three platform
packages before publishing the draft manually.

## Release from local machines

Use the Apple Silicon Mac for macOS ARM64, the Windows x64 machine for Windows,
and an **Ubuntu 24.04 x64 Docker container on Windows/Linux** for Linux. An ARM Linux VM on the Mac
would produce ARM binaries, which are outside the initial package matrix.

1. Set `VERSION.txt` to `X.Y.Z` or `X.Y.Z-rc.N`. Commit/merge the release changes,
   create the matching `vX.Y.Z` tag and push it when ready. For example:

   ```sh
   git tag v0.1.0-rc.1
   git push origin v0.1.0-rc.1
   ```

   The **Desktop release** workflow validates VERSION and default-branch ancestry,
   then creates a **source-only GitHub draft** with the exact source archive and
   checksum. It does not wait for native builders or publish anything publicly.
   The same workflow can be dispatched with an existing tag.

2. On **each** machine, fetch and check out the same tag. The build script refuses
   uncommitted source changes so binaries cannot be mislabeled with a commit they
   weren't built from. The unrelated local `output/` directory is excluded.

   ```sh
   git fetch origin --tags
   git checkout v0.1.0-rc.1
   python3.12 -m venv build-release-python
   # Windows: py -3.12 -m venv build-release-python
   # Activate: build-release-python\Scripts\Activate.ps1
   # Unix: source build-release-python/bin/activate
   python -m pip install "html5lib==$(python -c 'import json; print(json.load(open("dependencies.json"))["html5lib"])')"
   python tools/release/preflight.py --signing
   python tools/release/build.py --package --jobs 24
   ```

   In PowerShell install html5lib using the pin without shell substitution:

   ```powershell
   $pins = Get-Content dependencies.json -Raw | ConvertFrom-Json
   python -m pip install "html5lib==$($pins.html5lib)"
   rustup toolchain install $pins.rust --profile minimal
   python tools/release/preflight.py --signing
   python tools/release/build.py --package --jobs 24
   ```

   Install the pinned Rust toolchain on Unix too (`rustup toolchain install
   VERSION --profile minimal`, using `dependencies.json`). The build script selects
   it for its own process without changing your global/default toolchain.
   Run from a Windows MSVC Developer PowerShell or Developer Command Prompt.
   To reuse an already built SDK, add `--sdk-prefix /path/to/sdk`; it must have
   the exact matching build receipt or the command refuses it. This avoids another
   Chromium build when the current SDK is already verified.
   Packages and individual `.sha256` files appear in **`build-release-artifacts/`**.
   Retrying requires removing/moving the prior staged package directory of the
   same name; packaging refuses to overwrite it. Keep the archives/checksums.

   For an explicit **unsigned testing draft**, omit `--signing` from preflight and
   add `--unsigned` to the build command. macOS still uses local ad-hoc signatures
   needed by Apple Silicon; the package name includes `-unsigned`. Missing signing
   configuration fails before the expensive build instead of silently downgrading.

3. Copy the three archives and their `.sha256` files into one directory, e.g.
   `build-release-artifacts/` on the Mac. Keep filenames unchanged. In the clean
   checkout of the same release tag, sign into GitHub (`gh auth login`) and run:

   ```sh
   python tools/release/publish.py --tag v0.1.0-rc.1 --assets build-release-artifacts
   # Add --unsigned when collecting unsigned testing packages.
   ```

   The uploader checks each checksum, embedded version/commit, platform,
   architecture, signing status, dependency pins and engine fingerprint. It
   refuses a moved remote tag, mixed build versions, missing platform packages,
   or changes to a published release. It creates/updates a **draft** containing
   all three packages, the exact Arbiter source archive and **SHA256SUMS**.
   A source-only draft can also be prepared locally with `--prepare-only`.

4. Download the packages onto clean machines **without a Qt SDK**. Test onboarding,
   imports, passwords and biometric fallback, local passkeys/phone QR, workspace
   isolation/private windows, extensions/capture, ad blocking, PDF/printing,
   downloads, OS sharing/permissions, sleep/resume and native media playback.
   Real hardware/OS authorization tests cannot be replaced by CI mocks.
   Review corresponding sources/notices, then publish the draft manually in GitHub.

Windows users extract the entire ZIP and run `bin/arbiter.exe`. Linux users extract
and run `./arbiter`. The Linux archive targets the Ubuntu 24.04/glibc baseline and
retains system FFmpeg/OpenSSL/desktop dependencies; it does not claim to run on
every distribution. macOS packages require macOS 13+ and Apple Silicon.

## Machine prerequisites

Every builder needs the exact base Qt kit from `engine/manifest.json`, developer
and private headers, Qt Quick/Controls, WebChannel, Positioning, QtPdf, SVG,
PrintSupport and Linguist tools. Also install CMake 3.24+, Ninja, Git, Node.js as
required by the selected Qt release, Python 3.12 and the normal native Qt WebEngine
build prerequisites. Provide sufficient disk/RAM for Chromium. Default parallelism
is 24; use `--jobs 12` (for example) in a smaller VM.

Set these variables in the shell used to build:

- `ARBITER_QT_PREFIX`: absolute path to the exact installed base Qt kit.
- `ARBITER_OPENSSL_ROOT`: OpenSSL 3 prefix.
- `ARBITER_CMAKE_TOOLCHAIN`: optional vcpkg CMake toolchain (Windows).
- `ARBITER_OPENSSL_BIN`: Windows directory containing `libcrypto-3*.dll`.
- `ARBITER_ENGINE_CACHE`: optional persistent, machine-private SDK work directory.
  Otherwise the ignored `build-release-engine/` is used. The cache key includes
  engine inputs, host architecture and base-kit location. Directory names use a
  shortened fingerprint to stay within Ninja response-file filename limits; the
  SDK receipt still verifies the full fingerprint. Do not share writable
  SDK caches with untrusted processes/jobs. App/test build directories also follow
  the selected dependencies to avoid retaining stale CMake package paths.

macOS needs full Xcode 26+ with Icon Composer/`actool`, Command Line Tools and Metal.
Export `TOOLCHAINS` when the installed Metal component requires it (see
passkey-integration.md). Use an ARM64 Python/compiler session on the Mac.
Windows needs MSVC 2022, a Windows 11 SDK, `dumpbin`, `signtool` for signing,
OpenSSL development/runtime files, and Qt WebEngine's Windows prerequisites.
Linux needs system FFmpeg/OpenSSL development packages, Qt's build prerequisites,
`xvfb`, `dbus-x11`, `gnome-keyring`, `libsecret-1-dev`, `libopus-dev`, `patchelf`, `pkg-config`,
and `nm`. The Linux package bundles its Qt runtime and the matching ICU libraries
from the Qt kit; it does not rely on an incompatible distro Qt/ICU ABI. Do not run release browsers as root or disable Chromium's sandbox.

`python tools/release/preflight.py` checks basic architecture/tooling and the
provisioned Qt version; `--signing` also checks signing configuration names without
printing their values. Native configure/build remains the definitive prerequisite
and API compatibility check. Native local builds require ARBITER_QT_PREFIX. The Linux Docker image and optional
GitHub native workflow provision their own matching base kit.

For signing:

- **macOS:** import a Developer ID Application certificate/private key into the
  build keychain and set `ARBITER_MACOS_SIGNING_IDENTITY` to its identity. Store
  notarization credentials with `xcrun notarytool store-credentials`; set
  `ARBITER_NOTARY_PROFILE` to that keychain profile. Unlock the keychain and permit
  signing tools to use it. The pipeline signs nested code/helper entitlements
  before the outer app, notarizes, staples and recreates the ZIP.
- **Windows:** install a code-signing certificate/private key for the build account
  (or configure its hardware-backed provider) and set `ARBITER_WINDOWS_CERT_SHA1`.
  `ARBITER_TIMESTAMP_URL` optionally overrides the RFC 3161 timestamp endpoint.
  The executable is signed/verified; vendor dependency signatures are preserved.
- **Linux:** archives receive checksums, not Apple/Authenticode signatures.

Credentials are never committed. Keep the same signing identity between releases
so OS permission/keychain approvals can survive app upgrades. Unsigned packages
must not be presented as signed official releases.

## Pins, cache invalidation and checks

- `VERSION.txt` supplies the application/CMake/package version and must match tags.
- `engine/manifest.json` pins base Qt, WebEngine commit/version, Chromium submodule
  commit/version and Arbiter ABI capabilities. WebEngine and base Qt have
  **different version schemes**; numeric equality is not a compatibility test.
- `dependencies.json` pins Python, Rust, html5lib and the QtKeychain archive/version/SHA-256.
  Cargo/npm retain their own lockfiles; OpenSSL 3 and Linux FFmpeg come from the
  builder/distro. CMake/CI read these pins instead of repeating Qt versions.
- Patched builds reject a different base Qt version. Release builds use the pinned
  static QtKeychain instead of choosing an arbitrary installed shared copy.
- `engine/identity.py` hashes engine manifests, patches, overlays, build scripts
  and dependency pins. The SDK receipt is written only after pinned-source/version
  checks, compilation, codec policy verification and installation. Release CMake
  and packaging reject missing/stale receipts. ABI numbers alone are insufficient.
- A new input fingerprint selects a fresh SDK cache. Development builds may use
  older receipt-less SDKs; release builds require strict identity verification.
- Native builds run Python checks, Rust tests, the full native test suite (a
  disposable Linux keyring), QML lint and translation validation before packaging.
- Packages disable development SDK runtime overrides. Audits require the patched
  WebEngine export, helper/resources/locales and reject bundled CDM/codec files.
  macOS explicitly restores the patched engine after Qt deployment, thins universal
  dependencies to the native architecture, removes build-kit RPATHs, checks external
  library references and verifies nested signatures. Linux ELF RPATHs are relative.
- An off-the-record `--release-smoke-test` loads local HTML using the staged app and
  helper. It bypasses user profiles/settings/vaults and removes SDK paths from its
  environment. This is a package smoke test, not a replacement for clean-machine QA.

## Coordinated dependency updates

**Dependency maintenance** checks upstream stable Qt/Qt WebEngine tags weekly or
can be dispatched with explicit `qt` and `webengine_ref` inputs. It derives the
Chromium gitlink/version from that exact WebEngine commit and opens a **draft PR**.
It does not rebase/drop patches, merge, update local kits or publish binaries.

```sh
python tools/dependencies.py metadata
python tools/dependencies.py update --dry-run
python tools/dependencies.py update
python tools/dependencies.py update --qt 6.12.1 --webengine-ref v6.140.1
```

The explicit versions illustrate syntax; verify they exist upstream. Downgrades
and upstream prerelease refs are refused. A stable tag at the same version may
replace a development snapshot. Review upstream security/compatibility notes,
rebase `engine/patches/*.patch` and overlays as needed, install matching local Qt
kits, run native builds/tests on all three machines, then merge the update and
make a testing draft. Private API compatibility must be established by the builds
and runtime tests, never assumed from version numbers.

GitHub's **Allow Actions to create and approve pull requests** toggle is needed
for automatic draft PRs. Our workflows never approve/auto-merge PRs and retain
read-only default tokens. The workflow also uploads a `qt-dependency-candidate`
patch for manual review if a PR cannot be opened. Workflow-token PRs do not normally
trigger follow-up CI automatically; run checks manually or use a maintainer push.
Dependabot separately groups weekly Cargo, website npm and Actions updates.
Regenerate Cargo notices with `python tools/update-adblock-notices.py` when needed.
Update Python/Rust/html5lib/QtKeychain pins (and QtKeychain's verified archive hash) in
`dependencies.json`; review/test these alongside the Qt upgrade cycle.

## Optional self-hosted automation

The existing **Patched Qt WebEngine SDK** workflow and reusable `native-build.yml`
remain available if we later want unattended builds. They are not required by the
local release path. No Mac runner or startup service is registered by these files.
Use isolated builders with `self-hosted, macOS, ARM64, arbiter-webengine`,
`self-hosted, Windows, X64, arbiter-webengine`, and
`self-hosted, Linux, X64, arbiter-webengine` labels. Configure them through the
repository's **Settings → Actions → Runners → New self-hosted runner**.

The optional workflow uses the **`native-build` environment** with required
maintainer review, never untrusted pull-request triggers. Persistent self-hosted
runners can be compromised by untrusted jobs; see GitHub's guidance:
https://docs.github.com/en/actions/reference/security/secure-use
Default GitHub permissions stay read-only and checkout credentials are not retained.
If ARBITER_QT_PREFIX is absent it installs the manifest-pinned base kit; an explicit
kit override must match exactly. `ARBITER_BUILD_JOBS` can limit a smaller VM.
SDK archives preserve symlinks and builder CMake paths; they are diagnostic build
artifacts, not guaranteed relocatable SDK installers. Service accounts need the
same toolchain/keychain/GUI access as interactive native builds.

## Distribution notices and sources

Do not bundle/download Widevine or external Linux FFmpeg libraries. The SDK codec
policy check and package audit enforce this; native macOS/Windows codec negotiation
is preserved. See media-integration.md and the README DRM notice.

Arbiter is GPL-3.0-only. Packages include repository notices and the SDK SBOM when
available; Qt/Chromium/Rust/vendor components have additional licenses. The Arbiter
source archive includes pins and patches, **not the full source of all binary
dependencies**. Before publishing binaries, provide required corresponding
dependency sources/build material and generated third-party notices for the exact
SDK/toolchain. An SBOM or upstream link alone is not a claim of complete source
compliance. Qt deployment reference:
https://doc.qt.io/qt-6/qtwebengine-deploying.html
