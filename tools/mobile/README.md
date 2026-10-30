# Mobile maintenance tools

`test_peer.py` runs a phone client against the isolated native desktop TLS fixture.
It never reads a real user vault. See `mobile/README.md` for commands.

`sync_localizations.py` validates the eight offline catalogs, their placeholders
and protected names, then generates the credential extension's JSON catalogs and
native permission descriptions. Run it after editing `mobile/assets/i18n/*.json`.
The initial mobile catalogs reuse desktop translations and locally drafted new
strings. They need native-speaker review, especially security warnings.

`extract_strings.dart` uses the analyzer bundled with the pinned Flutter SDK to
wrap interface strings and extract interpolation placeholders. It preserves the
English master catalog (including native messages), so remove obsolete messages
manually from the master and localized catalogs. It must not be used to translate
page content or account names. Example from the repository root:

```sh
dart --packages="$FLUTTER_ROOT/packages/flutter_tools/.dart_tool/package_config.json" \
  tools/mobile/extract_strings.dart
python3 tools/mobile/merge_drafts.py
python3 tools/mobile/sync_localizations.py
```

`mobile-drafts.tsv` contains the new locally drafted translations, in the order
documented in its header. `merge_drafts.py` merges those into the current catalogs.
`translate_drafts.py` is an optional Google draft-generation tool for static UI
text only, requiring explicit permission to send new interface text externally.
It stops on rate limiting and preserves completed catalogs. No translation service
is used by the app.

## Ad-blocking assets

The app uses the source snapshots in `assets/adblock/`. It never downloads an
extension or filter engine at runtime. Regenerate Android's serialized filter data
and first-party extension bundle:

```sh
npm ci --ignore-scripts --prefix tools/mobile/adblock
node tools/mobile/adblock/build.mjs
node tools/mobile/adblock/notices.mjs
```

Regenerate iOS's native WebKit JSON using the pinned Brave filter converter:

```sh
CARGO_TARGET_DIR="$PWD/build-mobile-adblock" cargo run --locked --release \
  --manifest-path tools/mobile/safari-rules/Cargo.toml -- \
  assets/adblock/easylist.txt assets/adblock/easyprivacy.txt \
  > mobile/assets/adblock/webkit.json
```

WebKit cannot represent every ABP filter feature. The converter omits unsupported
rules. Always rerun `integration_test/adblock_test.dart` on both platforms after
changing source lists, converters or blocker libraries. That test proves a real
ad request is blocked while an ordinary script loads, and checks global and site
exceptions. It does not claim that every advertisement or tracker can be blocked.

`mobile/assets/browser/forms.js` is the common isolated credential/activity script.
After editing it, copy it to both native resource locations:

```sh
cp mobile/assets/browser/forms.js mobile/ios/Runner/BrowserForms.js
cp mobile/assets/browser/forms.js mobile/android/app/src/main/assets/arbiter-credentials/forms.js
```

No page-message listener is installed. Native code checks the sending frame,
session and origin before forwarding credential events or delivering credentials.
