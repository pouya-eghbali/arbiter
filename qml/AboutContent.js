.pragma library

// Shared product credits and notices; retain the AboutPage translation context.
function intro() { return qsTranslate("AboutPage", "An open-source browser for power users. Zero telemetry. Zero data collection."); }
function contribute() { return qsTranslate("AboutPage", "Built for people who like to make their browser their own. Everyone is welcome to contribute: share an idea, report a bug, improve the docs, or help build what comes next."); }
function licenseNotice() { return qsTranslate("AboutPage", "Licensed under the GNU General Public License, version 3 (GPL-3.0-only)."); }
function widevineNotice() { return qsTranslate("AboutPage", "Arbiter does not bundle or download Widevine. When available, it can use a compatible Widevine module already installed on your device. Widevine is proprietary Google software governed by applicable Google license terms. Compatibility with streaming services and playback quality are not guaranteed."); }
function creditsTitle() { return qsTranslate("AboutPage", "Made possible by"); }
function creditsThanks() { return qsTranslate("AboutPage", "Thank you to the contributors and open-source projects behind Arbiter."); }

function credits() {
    return [
            { name: "Qt & Qt WebEngine", detail: qsTranslate("AboutPage", "Application framework, interface, and Chromium-based web engine"), license: qsTranslate("AboutPage", "Qt licensing"), url: "https://www.qt.io/licensing/" },
            { name: "Chromium", detail: qsTranslate("AboutPage", "Web platform and browser engine"), license: qsTranslate("AboutPage", "BSD-style & third-party licenses"), url: "https://chromium.googlesource.com/chromium/src/+/main/LICENSE" },
            { name: "QtKeychain", detail: qsTranslate("AboutPage", "Secure storage through the operating system"), license: "BSD-3-Clause", url: "https://github.com/frankosterfeld/qtkeychain" },
            { name: "OpenSSL", detail: qsTranslate("AboutPage", "Cryptography for passwords and passkeys"), license: "Apache-2.0", url: "https://www.openssl.org/" },
            { name: "adblock-rust · Brave", detail: qsTranslate("AboutPage", "Ad and tracker blocking engine"), license: "MPL-2.0", url: "https://github.com/brave/adblock-rust" },
            { name: "EasyList & EasyPrivacy", detail: qsTranslate("AboutPage", "Community-maintained ad and tracker filter lists"), license: "CC BY-SA 3.0", url: "https://easylist.to/" },
            { name: "Lucide", detail: qsTranslate("AboutPage", "Interface icons by Lucide and its contributors"), license: "ISC", url: "https://lucide.dev/" },
            { name: "toml++ · Mark Gillard", detail: qsTranslate("AboutPage", "TOML configuration parsing"), license: "MIT", url: "https://github.com/marzer/tomlplusplus" },
            { name: "QR Code generator · Project Nayuki", detail: qsTranslate("AboutPage", "QR codes for signing in with another device"), license: "MIT", url: "https://www.nayuki.io/page/qr-code-generator-library" },
            { name: "SQLite", detail: qsTranslate("AboutPage", "Local history and browser data storage through Qt SQL"), license: qsTranslate("AboutPage", "Public domain"), url: "https://sqlite.org/" },
            { name: "libsecret", detail: qsTranslate("AboutPage", "Access to Linux secret storage"), license: "LGPL-2.1-or-later", url: "https://gitlab.gnome.org/GNOME/libsecret" }
        ];
}
