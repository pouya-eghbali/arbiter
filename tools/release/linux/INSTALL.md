# Install Arbiter for Linux

This download is for **Linux x86_64 with FFmpeg @FFMPEG_MAJOR@** and glibc 2.39
or later. It includes Qt; codecs come from your distribution.

Required FFmpeg shared libraries:

@FFMPEG_LIBRARIES@

Choose a download matching your installed FFmpeg major. A newer minor or patch
release within that ABI is compatible; a different major is a different ABI.
Do not rename or symlink libraries between major versions.

## Install

Extract the entire archive, open this folder in a terminal, then run:

```sh
./arbiter --check-dependencies
./install.sh
```

The installer requires Python 3. Run it as your normal desktop user, without
`sudo`. It installs the app in `~/.local/share/arbiter`, adds an **Arbiter**
entry and icon to your application menu, and creates `~/.local/bin/arbiter`.
If you set `XDG_DATA_HOME`, the app and desktop entry go there instead.

Open Arbiter from your application menu after installing. To use it without
installing, run `./arbiter` from this folder. Keep all the extracted folders
together; launching `bin/arbiter` directly bypasses the launcher.

Your desktop also needs its normal graphics/audio libraries, OpenSSL 3, and a
Secret Service or KWallet provider with an unlocked keyring for saved passwords.
The dependency check lists missing libraries; install them with your
distribution's package manager. Arbiter does not add repositories or download
codecs.

## Update

Close Arbiter, extract the newer matching download, and run its `./install.sh`.
Your profile, passwords, and settings are kept. The installer does not change
your default browser; select Arbiter in its settings or your desktop's default
applications settings.

## Uninstall

```sh
~/.local/share/arbiter/install.sh --uninstall
```

For a custom `XDG_DATA_HOME`, use `$XDG_DATA_HOME/arbiter/install.sh --uninstall`.
Browser data and saved credentials are kept.

To install elsewhere, use `./install.sh --prefix /your/prefix`. Pass the same
`--prefix` option when updating or uninstalling that installation.
