# Mobile development tools

`build.py` configures and builds the Qt/QML app. See `mobile/README.md` for kit,
signing and platform instructions. It does not upload or install artifacts.

`package_apk.py` aligns and signs a Release APK with your own keystore, verifies
the signature and 16 KB ZIP alignment, and writes its SHA-256. Passwords are read
from environment variables, never command arguments.

`merge_privacy.py` merges the application and linked static Qt SDK privacy
declarations for iOS; collection/tracking changes require an explicit review.

`sync_localizations.py` extracts Qt/QML and native interface strings, reuses
existing bundled translations, and checks the offline JSON catalogs. Missing
translations fall back to English; no translation service is contacted.

The credential/activity script at `mobile/assets/browser/forms.js` is bundled by
CMake. Native adapters inject it in an isolated world where supported, and verify
the sending frame and origin before handling credential events.

`draft_translations.py` optionally sends static interface text to Google Translate
for draft translations. Obtain user approval before running it. The app itself
never calls a translation service.

`test_native_crypto.py` checks Swift signatures, backup flags and encryption
envelopes against public vectors, without opening the Keychain.

## WebKit filter rules

Use the existing Rust crate's optional converter; Android and WebEngine use its
native engine directly. Regenerate iOS rules from the repository root:

```sh
CARGO_TARGET_DIR="$PWD/build-mobile-adblock" cargo run --locked --release \
  --manifest-path rust/adblock/Cargo.toml --features content-blocking \
  --bin arbiter-webkit-rules -- \
  assets/adblock/easylist.txt assets/adblock/easyprivacy.txt \
  > mobile/assets/adblock/webkit.json
```

WebKit cannot represent every ABP filter feature. Test a blocked third-party
request, a permitted first-party script, and global/site exceptions on both
platforms after changing lists or converters.
