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

For a native `.rpm` or `.deb` download matching your distro/FFmpeg version,
use your package manager so it can resolve dependencies. On openSUSE:

```sh
sudo zypper install ./arbiter-*.x86_64.rpm
```

On Ubuntu/Debian, use `sudo apt install ./arbiter_*_amd64.deb`.

This installs the browser, desktop entry, and icon system-wide. To uninstall,
use `sudo zypper remove arbiter`; your browser profile and credentials are kept.
If switching from `./install.sh` to an RPM, uninstall the user-installed copy
first so its application-menu entry does not take precedence over the RPM.
The 0.3.1 release has only the older tar archive; RPMs apply to builds produced
by the updated packaging scripts, as do DEBs and Windows setup executables.

### Portable archive

The current native archive targets **Ubuntu 24.04 x86_64**, with glibc 2.39 or
later and **FFmpeg 6** shared libraries. Other distributions need those same
library ABIs; having a newer FFmpeg alone does not make the archive compatible.

1. Download the Linux `.tar.xz` archive.
2. Extract the entire archive, keeping its folders together.
3. Open the extracted folder in a terminal.
4. In bundles containing `install.sh`, run `./install.sh` (requires Python 3).
   This installs Arbiter for your user, adds its icon to the application menu,
   and provides `~/.local/bin/arbiter`. No `sudo` is needed. With a custom
   `XDG_DATA_HOME`, application files and the desktop entry go there instead.
5. Open **Arbiter** from your application menu. You can also run `./arbiter`
   from the extracted folder without installing it.

The **0.3.1 archive does not include the installer**. For that archive, run
`./arbiter` from the extracted folder. Installing a desktop entry does not fix
missing runtime libraries.

The archive includes Qt. Your distribution supplies FFmpeg and OpenSSL 3,
graphics/audio libraries, and a Secret Service or KWallet password store.
An unlocked desktop keyring is required to save passwords. Run Arbiter as your
normal desktop user.

### Missing FFmpeg libraries

The 0.3.1 engine requires `libavcodec.so.60`, `libavformat.so.60`, and
`libavutil.so.58`. These belong to FFmpeg 6. For example, Tumbleweed systems
with only FFmpeg 8 (`libavcodec.so.62`) cannot run that engine as shipped.
Do not symlink one major version to another: FFmpeg major versions can change
both function interfaces and structure layouts.

On Ubuntu 24.04, install the runtime dependencies with:

```sh
sudo apt install libavcodec60 libavformat60 libavutil58 libopus0 libssl3t64
```

On other distributions, use a build targeting your distribution's FFmpeg ABI
or compatible packages supplied by that distribution. The archive does not
install repositories or download codecs. Avoid replacing your system's newer
FFmpeg with an older one just for Arbiter.

For dependency diagnostics, newer bundles support `./arbiter --check-dependencies`.
With 0.3.1, run `ldd bin/arbiter` from the extracted folder and look for
`not found` entries. Other platform dependencies, such as display libraries,
must also be available.

### Update or uninstall an installed bundle

Close Arbiter, extract the new download, and run its `./install.sh` again.
It replaces the application files while keeping your browser profile.
To remove a default installation, run:

```sh
~/.local/share/arbiter/install.sh --uninstall
```

If you used `--prefix /custom/prefix`, pass that same option when updating or
uninstalling. For a custom `XDG_DATA_HOME`, use the installer in
`$XDG_DATA_HOME/arbiter` to uninstall. Uninstalling keeps browser data and
credentials. You can select Arbiter as your default browser separately through
its settings or your desktop's default-applications settings.

## Windows

When a release provides a `-setup.exe`, run it to install Arbiter for your
Windows account. It adds a Start menu entry and an entry in Installed apps for
uninstalling. Administrator access is not required. The setup registers Arbiter
as an available default browser; choose it separately in Windows settings.
Close Arbiter before installing an update. Uninstalling keeps your browser data.

The ZIP remains available for portable use:

1. Download the Windows x64 `.zip` file.
2. Extract the entire archive, keeping its folders together.
3. Open `bin/arbiter.exe` in the extracted folder.

The Windows package is currently unsigned. Windows may display an unknown
publisher warning. Windows Hello vault verification requires Windows 11.

## Verify a download

Compare a file’s SHA-256 hash with its `.sha256` file or the release’s
`SHA256SUMS` manifest. On macOS, use `shasum -a 256 <file>`; on Linux,
use `sha256sum <file>`.

## Update

Download the newer version and replace the app (macOS) or extracted program
folder (Windows/Linux). Your normal browser data is stored separately from the app.
