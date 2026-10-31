# Windows cross-build (experimental)

The cross-build path targets **Windows x64 with the MSVC ABI**, using native
Clang/LLD on macOS or Linux. It does not run `cl.exe`, `windeployqt.exe`, Windows
Qt tools, or Windows tests on the host. MinGW Qt kits are incompatible with
Qt WebEngine and are not used.

This is an experimental build path, separate from the native release scripts.
Windows PE/Qt compile-and-link probes have passed on Apple Silicon, including
native `moc`/`rcc`, Windows resources, STL, C++/WinRT and static OpenSSL linkage. The patched
WebEngine configures and generates its GN build graph; a complete browser
package and Windows runtime validation are still required before release.
An executable compiling successfully does not establish that it runs correctly.

## Prerequisites

- Python 3.12, CMake, Ninja, Perl, GNU make, Git, Node, `patch`, and Rust/rustup
  at the versions in `dependencies.json`.
- Matching **LLVM 22.x and LLD 22.x** native installations. On Homebrew these
  are separate `llvm@22` and `lld@22` prefixes. The driver rejects mismatches.
- A native Qt kit matching `engine/manifest.json`, including WebEngine host GN
  and Qt host tools. On this Mac: `$HOME/Qt/6.12.0/macos`.
- Space for another Chromium source tree, build objects, Windows Qt, and SDK.
  Use an uncompressed local filesystem. Chromium source/build/tool paths should
  not contain spaces. A case-insensitive macOS filesystem works with xwin's
  case-insensitive layout; Linux needs xwin's generated case aliases.

The first preparation downloads Microsoft's SDK/CRT headers and import libraries
with [xwin](https://github.com/Jake-Shadle/xwin). Read and accept Microsoft's
[license terms](https://go.microsoft.com/fwlink/?LinkId=2086102) before passing
the explicit acceptance flag. The tool's own open-source license does not
replace Microsoft's terms. SDK, CRT, xwin and aqt versions are pinned in
`tools/cross/versions.json`; Qt follows our existing engine manifest.

## Commands

Run from the repository root. For example, in zsh/bash on Apple Silicon:

```sh
cross_args=(
  --host-qt "$HOME/Qt/6.12.0/macos"
  --llvm /opt/homebrew/opt/llvm@22
  --lld /opt/homebrew/opt/lld@22
  --jobs 20
)

# Once, after accepting the linked Microsoft terms:
python3.12 tools/cross/windows.py prepare "${cross_args[@]}" --accept-microsoft-license

# Cheap toolchain/Qt check, without Chromium compilation:
python3.12 tools/cross/windows.py probe "${cross_args[@]}"

# Optional configure-only check, then the expensive incremental engine build:
python3.12 tools/cross/windows.py engine "${cross_args[@]}" --configure-only
python3.12 tools/cross/windows.py engine "${cross_args[@]}"

# Checksum-pinned OpenSSL, built for the same Windows ABI and /MD runtime:
python3.12 tools/cross/windows.py openssl "${cross_args[@]}"

# Requires the completed Windows engine SDK and OpenSSL SDK:
python3.12 tools/cross/windows.py app "${cross_args[@]}"

# Alternatively, run the entire sequence. Preparation is reused if its pins match.
python3.12 tools/cross/windows.py all "${cross_args[@]}" --accept-microsoft-license
```

Stages use `build-windows-cross/` by default (`--work` overrides it). Downloads,
SDKs and binaries are ignored by Git. Do not run stages concurrently in the
same directory; the driver holds a lock to prevent SDK preparation replacing
headers while compilation is running. Preparation can be repeated, but it
re-extracts SDK/Qt files; normal incremental builds only need `engine` or `app`.
Redirect output to a file if desired, for example `> build-windows-cross/engine.log 2>&1`.

Outputs:

| Path inside the work directory | Purpose |
| --- | --- |
| `probe/windows-probe.exe`, `probe/qt-probe.exe` | Link probes, not the browser |
| `source/` | Isolated, pinned and patched WebEngine source |
| `engine/` | Incremental Windows engine build |
| `sdk/` | Installed engine, only stamped after build/install/media audit succeed |
| `openssl/sdk/` | Windows static OpenSSL libraries, headers and license |
| `app/arbiter.exe` | Application executable; not a deployed distribution |

## Host/target separation and integrity

`cmake/toolchains/WindowsClang.cmake` uses the Microsoft headers/libraries from
xwin and native LLVM tools to emit Windows COFF objects. It keeps host program
discovery separate from target library/package discovery. `QT_HOST_PATH` selects
native Qt generators; `CMAKE_PREFIX_PATH` selects the Windows Qt kit.

`tools/cross/qtwebengine.patch` adapts the pinned Qt build machinery: native
host/V8 tools, GN path handling, LLVM archiver, explicit Windows include/library
flags, and xwin instead of `vcvarsall.bat`. It is applied only to the isolated
cross-build source, after the existing engine preparer verifies the base
revision, Arbiter patches and overlays. Native SDK fingerprints are unchanged.

The cross SDK carries the usual engine identity plus a separate cross-toolchain
receipt. The application stage rejects a macOS/Linux SDK, stale engine inputs,
or a changed cross toolchain. Failed/incomplete engine builds do not carry a
successful engine receipt. Windows media policy stays codec-free FFmpeg plus
native Windows decoders, even when the build host is Linux. Widevine is not
downloaded or bundled.

OpenSSL uses Clang's `x86_64-pc-windows-msvc` target, Microsoft headers, and the
dynamic Microsoft CRT. OpenSSL's `mingw64` configuration supplies its portable
make rules; no MinGW toolchain or runtime is involved. The build rejects MinGW
runtime references and checks archive architecture. Assembly, shared OpenSSL
DLLs and loadable modules are disabled for this path. Keep the bundled OpenSSL
license when eventually packaging the application. `--openssl-prefix` may select
another compatible **static** Windows SDK; host Homebrew OpenSSL cannot be used.

## Release limitations

This driver does **not** package, sign, upload, or publish a release. Qt's native
Windows deployer and the existing Windows signing/GUI smoke tests still need a
Windows environment. A Windows VM or a small hosted Windows job can eventually
perform those final steps without compiling Chromium there. Until that stage
is validated, use the native Windows release path for published Windows builds.
Do not upload the link probes or the bare `arbiter.exe` as release artifacts.

Build-script tests are small and do not compile Qt/Chromium:

```sh
python3.12 -m unittest discover -s tools/cross -v
```
