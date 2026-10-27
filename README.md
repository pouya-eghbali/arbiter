<p align="center">
  <img src="assets/brand/arbiter-badge.svg" width="88" alt="Arbiter logo" />
</p>

<h1 align="center">Arbiter</h1>
<p align="center"><strong>The web. On your terms.</strong></p>
<p align="center">
  <a href="https://arbiterbrowser.com">Website</a> ·
  <a href="https://github.com/pouya-eghbali/arbiter/releases/latest">Download</a> ·
  <a href="https://github.com/pouya-eghbali/arbiter/issues">Report an issue</a>
</p>

Arbiter is an open-source desktop browser for power users. A floating navigation
pill, flexible split views and workspaces give you room to browse your way.
Use the mouse, make it keyboard-first, or hide the controls entirely.

**Free and open source. Zero telemetry. No data collection.**

## Download

| Platform | Download | Requirements |
| --- | --- | --- |
| macOS | [DMG](https://github.com/pouya-eghbali/arbiter/releases/download/v0.1.0/arbiter-0.1.0-macos-arm64.dmg) | Apple Silicon · macOS 14.4 or later |
| Linux | [Archive](https://github.com/pouya-eghbali/arbiter/releases/download/v0.1.0/arbiter-0.1.0-linux-x86_64.tar.xz) | x86_64 · Ubuntu 24.04 or compatible |
| Windows | Coming soon | The download will be added to the same release. |

On **macOS**, open the DMG and drag Arbiter to Applications. The app is signed
and notarized. On **Linux**, extract the archive and run `./arbiter` from the
extracted folder. See the [installation guide](docs/installing.md) for Linux
system packages and troubleshooting.

[Release notes](docs/release-notes/v0.1.0.md) ·
[All releases and checksums](https://github.com/pouya-eghbali/arbiter/releases)

## A browser that fits your workflow

- **More page, less chrome.** Bring up the floating pill when you need it, or
  keep it out of the way. Mouse controls and customizable shortcuts work together.
- **Workspaces for every context.** Organize tabs by project, work or play.
  Choose separate sign-in sessions when you need them.
- **Split without limits.** Split horizontally or vertically, resize panes and
  swap them by dragging. Put pages from different workspaces side by side.
- **Your logins, close at hand.** Save, generate and autofill passwords using
  your operating system’s credential store. Use Arbiter passkeys or sign in
  with a passkey on your phone by scanning a QR code.
- **Private windows that lock.** Blur and darken private browsing when you step
  away, with biometric or PIN/password unlocking.
- **Less noise.** Built-in ad blocking, with a quick toggle for the current page.
  Idle tabs sleep while visible panes and tabs playing audio stay awake.
- **Make it yours.** Light, dark and system themes; separate accent colors,
  background colors and images for each mode; customizable keyboard shortcuts.
- **Bring your essentials.** Import passwords and history, install Chrome Web
  Store extensions, view PDFs, print, manage downloads and share through your OS.

Arbiter’s saved credentials are local to each computer; cross-device sync is
not available yet. Extension compatibility varies.

## Getting started

The first-run setup helps you import from another browser, choose your layout
and colors, and optionally make Arbiter your default browser.

| Action | macOS | Windows / Linux |
| --- | --- | --- |
| Search or enter an address | `⌘ L` | `Ctrl L` |
| New tab | `⌘ T` | `Ctrl T` |
| Tab overview | `⌘ Shift A` | `Ctrl Shift A` |
| Workspaces | `⌘ Shift E` | `Ctrl Shift E` |
| Settings | `⌘ ,` | `Ctrl ,` |

You can change shortcuts in **Settings → Keyboard**. Appearance, privacy,
passwords and the default search engine are configurable in Settings too.

## Streaming and DRM

Arbiter does not bundle or download Widevine. When available, it can use a
compatible Widevine module already installed on your device. Widevine is
proprietary Google software governed by applicable Google license terms.
Compatibility with streaming services and playback quality are not guaranteed.

## Contribute

Anyone is welcome to contribute: code, translations, design, bug reports and
ideas. [Open an issue](https://github.com/pouya-eghbali/arbiter/issues) to discuss
a change or tell us what could be better.

- [Build and run Arbiter](docs/development.md#build-and-run)
- [Release and dependency maintenance](docs/releases.md)
- [Contribute translations](docs/translations.md)
- [Work on the website](website/README.md)

## License

Arbiter is licensed under [GPL-3.0-only](LICENSE). Third-party components retain
their own licenses; see [third-party notices](licenses/). UI icons are from
[Lucide](https://lucide.dev/).
