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

The default work directory is `build-linux-cross` (ignored by Git). Individual
phases are `prepare`, `probe`, `runtime`, `engine` and `app`. `engine
--configure-only` checks the CMake configuration without compiling Chromium.
Outputs are `build-linux-cross/sdk` and `build-linux-cross/app/arbiter`.

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
LLVM's Linux compiler builtins are cross-compiled from its checksum-pinned source.
The native Mac Qt tools generate code; Linux Qt libraries are linked into target
binaries. Linux media uses the target system's FFmpeg rather than bundling it.

## Validation and packaging

Do not run the generated ELF executable on macOS. Test it on supported Linux x64,
including Qt platform plugins, WebEngine subprocesses, graphics, password storage,
media, extensions and PDF rendering before publishing. Cross compilation reduces
build time; it does not replace native runtime tests. The existing Docker build
remains available separately for developers who prefer a Linux build environment.
