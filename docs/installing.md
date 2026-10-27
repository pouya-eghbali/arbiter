# Install Arbiter

Download an official build from [GitHub Releases](https://github.com/pouya-eghbali/arbiter/releases/latest).

## macOS

Requires an Apple Silicon Mac running macOS 14.4 or later.

1. Download and open the `.dmg` file.
2. Drag **Arbiter** to **Applications**.
3. Open Arbiter from Applications and follow the short setup.

The application and disk image are signed and notarized. The ZIP download
contains the same app if you prefer that format.

## Linux

Requires an x86_64 desktop running Ubuntu 24.04 or a compatible distribution
with glibc 2.39 or later.

1. Download the Linux `.tar.xz` archive.
2. Extract the entire archive, keeping its folders together.
3. Open the extracted folder and run `./arbiter`.

The archive includes Qt. Your distribution supplies FFmpeg and OpenSSL 3,
graphics/audio libraries, and a Secret Service or KWallet password store.
An unlocked desktop keyring is required to save passwords. If Arbiter reports
a missing shared library, install that library through your distribution’s
package manager. On Ubuntu, `ldd bin/arbiter` from the extracted folder can
help identify missing packages. Run Arbiter as your normal desktop user.

## Windows

The Windows build will be added to the release when ready.

## Verify a download

Compare a file’s SHA-256 hash with its `.sha256` file or the release’s
`SHA256SUMS` manifest. On macOS, use `shasum -a 256 <file>`; on Linux,
use `sha256sum <file>`.

## Update

Download the newer version and replace the app (macOS) or extracted program
folder (Linux). Your normal browser data is stored separately from the app.
