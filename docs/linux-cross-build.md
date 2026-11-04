# Linux x64 cross-build on macOS

The experimental cross-build uses native macOS Clang, LLD, CMake, Ninja, Python,
Qt host tools and Rust to produce Linux x64 binaries. It does not use Docker or
execute Linux programs through emulation. A Linux machine is still required for
runtime testing and release validation.

## Requirements

- Apple Silicon macOS with Xcode command-line tools.
- The matching native Qt kit from `engine/manifest.json` (currently 6.12.0).
- LLVM and LLD 22.1.8, CMake, Ninja, pkg-config, GnuPG, zstd, Python 3.12 and Rust.
- Space for a second Chromium source/build tree and the target SDK.

For Homebrew: `brew install llvm@22 lld@22 cmake ninja pkgconf gnupg zstd python@3.12`.

```sh
python3.12 tools/cross/linux.py all \
  --host-qt "$HOME/Qt/6.12.0/macos" \
  --llvm /opt/homebrew/opt/llvm@22 \
  --lld /opt/homebrew/opt/lld@22 \
  --jobs 16
```

On macOS, `./tools/release/build-linux.sh --jobs 16` selects this direct cross-build
by default. `ARBITER_QT_PREFIX`, `ARBITER_LINUX_LLVM` and `ARBITER_LINUX_LLD` can
override the native kit/tool prefixes. Set `ARBITER_LINUX_DOCKER=1` only to
explicitly choose the older Docker alternative. This entry point compiles;
validation and packaging still run on Linux x64.

The default work directory is `build-linux-cross` (ignored by Git). Individual
phases are `prepare`, `probe`, `runtime`, `engine` and `app`. `engine
--configure-only` checks the CMake configuration without compiling Chromium.
Outputs are `build-linux-cross/sdk` and `build-linux-cross/app/arbiter`.

### Native FFmpeg ABI variants

The default `--ffmpeg 6` targets Ubuntu 24.04's `libavcodec.so.60`,
`libavformat.so.60`, and `libavutil.so.58`. Newer FFmpeg majors are separate
ABIs, not drop-in replacements. Current openSUSE Tumbleweed ships FFmpeg 9
(`libavcodec.so.63`,
`libavformat.so.63`, `libavutil.so.61`). Build for that ABI explicitly:

```sh
python3.12 tools/cross/linux.py engine --ffmpeg 9 \
  --host-qt "$HOME/Qt/6.12.0/macos" \
  --llvm /opt/homebrew/opt/llvm@22 --lld /opt/homebrew/opt/lld@22 --jobs 24
python3.12 tools/cross/linux.py app --ffmpeg 9 \
  --host-qt "$HOME/Qt/6.12.0/macos" \
  --llvm /opt/homebrew/opt/llvm@22 --lld /opt/homebrew/opt/lld@22 --jobs 24
```

Run `prepare` and `runtime` first in a new work directory. Each SDK is kept separately: `sdk`, `sdk-ffmpeg8`, and `sdk-ffmpeg9`,
with corresponding applications in `app`, `app-ffmpeg8`, and `app-ffmpeg9`. The engine compilation directory is shared, so switching ABIs
recompiles affected media code and relinks instead of rebuilding all Chromium.

The FFmpeg 8 and 9 profiles build checksum-pinned, **compile-only** link SDKs
under `sysroot/usr/local/arbiter-ffmpeg8` and `sysroot/usr/local/arbiter-ffmpeg9`.
Every codec is disabled in these SDKs. Their headers and import libraries select the target ABI; none of its `libav*`
libraries enter the application bundle. The user's distribution supplies the
actual decoders. Never put that compile-only SDK on a runtime library path.
The driver checks the resulting engine's ELF dependencies before writing its
ABI receipt; the packager checks them again.

Cross builds use Chromium's maintained XML/XSLT parsers rather than linking
Ubuntu's `libxml2.so.2`, which is absent on current Tumbleweed. The parser source
and notices are part of the pinned Chromium tree. Media codecs remain external.

Native packaging derives the archive name from the actual engine dependencies:
the existing Ubuntu/FFmpeg 6 archive keeps its name; FFmpeg 8 and 9 add
`-ffmpeg8.tar.xz` and `-ffmpeg9.tar.xz`, respectively. The archive's
`release.json` and installation instructions
list the required libraries. A native FFmpeg 7 SDK is also recognized and
labeled `-ffmpeg7`; the macOS cross driver currently prepares 6, 8, and 9.
Keep the older profile only for systems
actually using that older runtime; do not infer the current distro default from
a compatibility library left installed on one machine.

To prepare inputs for native validation of the FFmpeg 9 variant:

```sh
INPUT_PLATFORM=linux ARBITER_CROSS_SDK="$PWD/build-linux-cross/sdk-ffmpeg9" \
  python3.12 tools/release/cross_inputs.py pack
```

Upload that private `build-inputs-linux-ffmpeg9.zip` to the matching release
draft, then select `linux_ffmpeg: 9` in the manual cross-SDK validation workflow.
The disposable Ubuntu runner installs a test-only FFmpeg 9 runtime, builds and
tests Arbiter, and packages without codecs. FFmpeg 6 validation keeps using the
distro's system libraries. Tumbleweed startup/desktop checks are also required
before calling the FFmpeg 9 package supported there.

Upload validated variants using `tools/release/upload.sh --platform linux
--linux-ffmpeg 6 --linux-ffmpeg 8 --tag vX.Y.Z`. The uploader verifies ABI metadata
and distinguishes the archives and checksum manifest. A new engine ABI variant
must not replace an older published asset or be labeled as tested before its
runtime checks pass.

After validating an extracted bundle on a matching Linux distro, generate a
native RPM there with `python3 tools/release/linux_rpm.py /path/to/bundle --output
build-release-artifacts` (requires `rpm-build` and `patchelf`). The RPM records
external shared-library capabilities for the package manager to resolve,
excluding private bundled Qt libraries. It installs under `/opt/arbiter`, with
a launcher, desktop entry, and icon under `/usr`. Browser data stays outside
the RPM. Test `zypper install ./arbiter-*.rpm`, launch as a desktop user, and
remove via the package manager before publishing it.

`python3 tools/release/validate_linux_bundle.py /path/to/bundle` checks the ELF
ABI, verifies that no media codec libraries were bundled, installs into an
isolated temporary home, validates the desktop entry, starts an actual
WebEngine renderer under Xvfb/D-Bus, and uninstalls while checking that profile
data remains intact. Run it on the target distro as an unprivileged user with
`desktop-file-utils`, `ldd` (or `patchelf`), and `Xvfb` available. It uses a
separate D-Bus session too when `dbus-run-session` is installed.

The probe compiles and links Qt resources, Qt's meta-object system, C++ threads,
FFmpeg, NSS and OpenSSL into an ELF x64 executable. It verifies the executable's
architecture and dynamic dependencies; it does not claim runtime validation.

## Reproducible target libraries

`tools/cross/linux-sysroot.json` selects Ubuntu 24.04 amd64 development packages.
`linux-packages.lock.json` pins the complete dependency closure and every package
SHA-256. Lock generation verifies Ubuntu's signed repository metadata against
pinned Ubuntu archive-signing fingerprints. Preparation verifies each package,
extracts its files without running package scripts, and rewrites absolute
symlinks so they stay inside the sysroot. The sysroot is never added to the host
program search path.

To intentionally refresh this SDK, run `lock-sysroot` with the same arguments,
review the lock diff, and rebuild. Toolchain, sysroot and engine inputs are
fingerprinted so an SDK from different inputs cannot silently be reused.

WebEngine and Qt PDF come from the same pinned, patched source snapshot.
The driver explicitly selects a desktop build with WebRTC and browser printing/PDF
enabled; Qt's default Unix cross-build configuration targets embedded devices.
LLVM's Linux compiler builtins are cross-compiled from its checksum-pinned source.
The native Mac Qt tools generate code; Linux Qt libraries are linked into target
binaries. Linux media uses the target system's FFmpeg rather than bundling it.

## Validation and packaging

Do not run the generated ELF executable on macOS. Test it on supported Linux x64,
including Qt platform plugins, WebEngine subprocesses, graphics, password storage,
media, extensions and PDF rendering before publishing. Cross compilation reduces
build time; it does not replace native runtime tests. The existing Docker build
remains available separately for developers who prefer a Linux build environment.

Installing the correct major does not guarantee decoder availability. Restricted
and full codec providers can expose identical SONAMEs. Validate the packaged
engine with `tools/media-probe` local H.264/AAC playback on the target distro;
a working `ffmpeg` executable alone does not prove the browser loads that ABI.
