# Built-in ad blocking

Arbiter embeds Brave's `adblock` Rust crate (pinned to 0.13.3) behind a small
C interface. `rust/adblock/Cargo.lock` pins transitive dependencies. The engine
uses EasyList and EasyPrivacy; original snapshots, metadata, and upstream
commit IDs are kept in `assets/adblock/`.

## Controls

- **Settings → Privacy → Block ads and trackers** enables/disables blocking
  globally, including private windows. The default is on, stored as
  `[AdBlock] enabled = true` in the TOML preferences.
- The navigation pill's shield pauses/resumes blocking only in the focused
  view and reloads it. Another view of the same site remains protected.
- **Command+Option+B** on macOS, **Ctrl+Alt+B** on Windows/Linux does the same.
  Customize “Toggle ad blocking in focused view” in Keyboard settings. It works
  with the pill hidden.
- A pause belongs to that live tab/view, survives navigation, and disappears
  when the tab closes. It is not a persistent website allowlist. Global off
  takes precedence; re-enabling globally preserves existing view pauses.
- Reload existing pages after changing the global setting. Disabling removes
  injected styles immediately; previously blocked requests require a reload.

The shield tooltip includes a per-navigation blocked-request count. Counts and
view pauses are not written to history, preferences, or private-session storage.

## Lists and updates

Bundled lists provide protection offline from first launch. Downloaded lists
are cached under Qt's `AppLocalDataLocation/adblock/rules.txt`. Startup uses the
cache if available, otherwise the bundled snapshot. When enabled, missing or
four-day-old caches trigger a background update; **Update lists** updates them
on demand. This contacts only the two official HTTPS list endpoints and sends
no browsing URLs. Downloads have size/time/redirect limits and basic format
validation. Both downloads must succeed before a worker builds a replacement
engine. The previous engine remains active during download/compilation and
on failure; cache replacement uses `QSaveFile`.

## WebEngine integration

The Qt patch exposes `WebEngineView.urlRequestInterceptor`, backed by
`WebContentsAdapter::setRequestInterceptor`. The interceptor is preserved when
changing a profile or adopting a popup's WebContents. It uses a guarded pointer;
destroying the interceptor cannot leave a dangling adapter pointer.

Each view owns a `ViewAdBlock` interceptor. Shared profiles do not imply shared
pause state. Requests are classified by resource type and evaluated with their
first-party URL (or initiator). The user’s top-level navigation is allowed;
subframes and resources are filtered. Matching occurs before Chromium sends
the request. No blocking-path operation calls back into the profile or QML.
The immutable engine is shared by reference; matching and replacement are
synchronized. Rust's `single-thread` optimization is disabled and FFI panics
are caught.

Cosmetic hiding runs in ApplicationWorld in the main document. A bounded DOM
collector watches new classes/IDs and queries Brave's generic and site-specific
selectors, respecting exceptions. Styles are inserted individually so an
unsupported selector cannot invalidate the entire sheet. Document tokens stop
stale results from styling a new document. This initial integration implements
network blocking and standard CSS hiding; it does not execute procedural
filters/scriptlets, load redirect-resource replacements, rewrite URL parameters,
or inject cosmetic styles into subframes. Browser-generated/background service
requests without a view are outside the page interceptor's scope.

## Building

Install Rust 1.93+ with Cargo and the target matching your Qt build. CMake finds
Cargo on PATH, in `~/.cargo/bin`, or through `rustup which cargo`. It builds a
static library with `--locked --release --jobs 24`; Rust is a build dependency,
not a runtime requirement. `ARBITER_CARGO_HOME` selects the dependency cache.

Use the current patched SDK with `ARBITER_PATCHED_WEBENGINE=ON` and
`ARBITER_WEBENGINE_PREFIX=...`. Older SDKs must be rebuilt using `engine/build.py`;
the manifest records the required per-view interception hook. If a source
checkout still contains older patches, use a fresh `--work` directory or
reconcile those patches explicitly; the driver refuses to overwrite edits. Stock Qt Quick
does not expose this hook, so this build now requires the patched SDK.

For cross builds set `ARBITER_RUST_TARGET` explicitly (for example
`aarch64-apple-darwin`, `x86_64-pc-windows-msvc`, or
`x86_64-unknown-linux-gnu`). Install that target's Rust standard library and
linker. Build macOS architectures separately before combining a universal app.
macOS, Windows, and Linux share the same Rust/C++ filtering code; each platform
still needs its native build/runtime verification.

```sh
cargo test --manifest-path rust/adblock/Cargo.toml --locked --release
cmake --build build-patched-tests --target adblock_test keybindings_test --parallel 24
ctest --test-dir build-patched-tests -R '^(adblock|keybindings)$' --output-on-failure
```

Tests use synthetic rules and loopback pages with two simultaneous views of
the same URL/profile, verifying per-view isolation, global disable, exceptions,
third-party rules, bundled-list matching, and cosmetic execution. No real
accounts or browsing data are required.

## Attribution

Brave's engine is MPL-2.0; see `licenses/AdBlock.txt` and its
[upstream source](https://github.com/brave/adblock-rust/tree/v0.13.3).
EasyList/EasyPrivacy are attributed to The EasyList authors
(https://easylist.to/), used under CC BY-SA 3.0 Unported; the unmodified source
lists and full notice/license are included. See `licenses/EasyList.txt`.
Cargo dependency notices are embedded in `licenses/AdBlockDependencies.txt`.
Regenerate them with `tools/update-adblock-notices.py` after fetching/upgrading
the locked dependencies. Supplemental upstream license texts omitted from crate
archives are kept under `licenses/adblock-extra/`.
