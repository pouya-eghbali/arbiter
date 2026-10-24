# Password manager

This implementation manages local passwords on macOS, Windows, and Linux.
Passkeys, sync, imports/exports, an independent master password, and biometric
locking are outside this version.

## Storage

`SecretStore` isolates the controller from QtKeychain. Each login is a separate
binary OS-store item under service `Arbiter Passwords`. Keys are globally
namespaced with a hash of the profile directory/name: Windows Credential Manager
does not use QtKeychain's service as a namespace.

The CBOR record contains version, exact HTTPS origin, username, and password.
Origin and username must match the index before any fill is authorized. Changing
index metadata cannot redirect a saved password to another website.

`passwords/Default/index.json` below `QStandardPaths::AppLocalDataLocation`
contains UUIDs, sites, and usernames, but no passwords. This metadata is **not
encrypted**. Files/directories have owner-only permissions where supported;
Windows uses the user profile's ACLs. Passwords are not written to QSettings,
diagnostics, or logs.

QtKeychain uses macOS Keychain, Windows Credential Store, and supported Linux
keyrings. Every job sets `setInsecureFallback(false)`. There is no plaintext or
home-grown encryption fallback when a keyring is unavailable. The bundled Windows
configuration uses Credential Store; an externally provided QtKeychain may use its
encrypted DPAPI alternative. Records are limited to 2560 encoded bytes everywhere
to respect Windows Credential Store's blob limit.

Index changes use QSaveFile and QLockFile. New index entries are reserved before
storing secrets, avoiding undiscoverable secrets after a crash. Failed writes
roll back reservations. A crash can leave an indexed account whose secret needs
re-saving/deletion. Deletion removes the secret first; an index-write failure
leaves a retryable entry and reports an error. Corrupt/unsupported indexes and
concurrent processes fail closed rather than replacing data.

The index and OS store are a pair. Copying a profile does not transfer passwords;
moving the directory changes its credential namespace. There is no backup/restore
feature yet. Use Delete actions rather than removing the index to reset storage.

## Browser boundary

The form script runs at DocumentReady in ApplicationWorld, only in the main frame.
Only the write-only `PasswordCapture` object is published to its isolated
WebChannel. The manager, account lists, password reads, and fill methods are not
registered. The application validates reported origins against the engine's
committed main-frame URL before offering a save. Private captures are rejected.

Filling requires native account selection. The controller matches the exact
scheme/host/port and verifies the retrieved record's binding. It binds the fill
to an inspected document UUID and form ID. Navigation, tab switches, and destroyed
sessions invalidate pending reads. At execution, the script checks origin,
document/form identity, form action, and visible/connected inputs, including
rechecking after username input handlers run. It never submits forms.

Non-HTTPS origins, cross-origin form actions, subframes, hidden/disabled/read-only
inputs, and ambiguous unfocused forms are not filled. Existing passwords are not
filled into new-password fields. Ordinary and dynamic forms and submission capture
of form-less fields are supported. Username-first fields require
`autocomplete="username"` for filling; capture does not carry usernames between
separate documents, so users can correct the native save prompt. Closed shadow
DOM/custom widgets and HTTP authentication dialogs are not integrated.

Supported submissions offer Save/Update, suppressing unchanged saved passwords.
A submission is not proof of success; the prompt says to dismiss it if sign-in
failed. Sign-up/new-password forms participate in automatic submission capture;
there is no manual-save action in the account picker. Disabling saving/filling
retains credentials and still allows deletion.

The password-field context menu offers Suggest password. An isolated-world hit
test binds the clicked field to a document UUID, form token, and field token.
Native code validates the committed HTTPS origin and generates the password.
The script rejects stale documents, cross-origin actions, detached inputs, and
changed field/form associations. It fills only the chosen password field and,
when explicitly marked new-password, matching new-password confirmation fields.
It never overwrites current-password fields as part of a new-password suggestion.
Generation does not submit or save; normal submission capture offers to save.

Isolation does not hide filled field values from the website. Management does
not reveal existing passwords; Show reveals only a newly entered/generated value.
OS account/keyring policy still controls reads. Fingerprint verification defaults
on (`Passwords.biometricEnabled=true`) and is configurable only in Passwords
settings. When supported, Arbiter additionally verifies the user before autofill. Successful
verification can be reused for `Passwords.verificationCooldown` seconds (default
300, range 0–3600; 0 verifies every time), configurable in Passwords settings.
The interval starts at the successful verification, not the most recent fill.
Only a monotonic timestamp is held in memory. Restart, session lock/sleep,
disabling passwords, changing the cooldown, or biometric availability changes
invalidate it. Late verification/read callbacks cannot authorize fills after
invalidation. Disabling biometric verification clears its remembered approval.
Qt strings and DOM values cannot be guaranteed to be securely erased. Pending
capture data expires after two minutes or dismissal. There is no decrypted vault
cache.

## Build and validation

CMake prefers an installed Qt6Keychain 0.14.2+ package. Otherwise it fetches the
hash-verified 0.14.2 source archive and builds it statically. For offline builds,
use `-DFETCHCONTENT_SOURCE_DIR_QTKEYCHAIN=/path/to/qtkeychain-0.14.2`. This also
works for the independent tests project. Its license is installed with Arbiter.

Linux needs Qt DBus, pkg-config, libsecret development headers, and a working
Secret Service or supported KWallet session. Missing/locked keyrings produce
errors rather than plaintext storage. macOS packaging should keep a stable signing
identity for consistent Keychain access decisions.

macOS builds sign and verify the complete app bundle after linking. The default
ad-hoc signature (`ARBITER_MACOS_SIGNING_IDENTITY=-`) supports local development;
its identity changes when the executable changes. Set
`-DARBITER_MACOS_SIGNING_IDENTITY="<certificate name or SHA-1>"` to use a
certificate identity across builds. Packaging must sign again after deployment
changes the bundle. Signing does not merge the separate application Touch ID
check with login-Keychain authorization, or unlock a locked Keychain.

Tests cover persistence, failures, origin binding, index tampering/corruption,
concurrent access, private/disabled capture, size limits, generation, navigation
cancellation, and real WebEngine form handling. Set `ARBITER_TEST_KEYCHAIN=1` for
the disposable OS-store round trip. macOS requires Cocoa; offscreen does not drain
Apple's main dispatch queue. Headless Linux requires a disposable D-Bus/Secret
Service session.

The GitHub workflow defines macOS, Windows, and Linux tests, including a Linux
test keyring. Adding it does not establish that remote runs passed. Windows/Linux
runtime checks must pass on those hosts before claiming platform validation.
