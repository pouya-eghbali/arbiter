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
filled into new-password fields. Ordinary and dynamic forms and explicit capture
of form-less fields are supported. Username-first fields require
`autocomplete="username"` for filling; capture does not carry usernames between
separate documents, so users can correct the native save prompt. Closed shadow
DOM/custom widgets and HTTP authentication dialogs are not integrated.

Supported submissions offer Save/Update, suppressing unchanged saved passwords.
A submission is not proof of success; the prompt says to dismiss it if sign-in
failed. Users can explicitly capture unsupported submissions with Save current
login. Disabling saving/filling retains credentials and still allows deletion.

Isolation does not hide filled field values from the website. Management does
not reveal existing passwords; Show reveals only a newly entered/generated value.
Access relies on OS account/keyring policy, with no additional application lock.
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

Tests cover persistence, failures, origin binding, index tampering/corruption,
concurrent access, private/disabled capture, size limits, generation, navigation
cancellation, and real WebEngine form handling. Set `ARBITER_TEST_KEYCHAIN=1` for
the disposable OS-store round trip. macOS requires Cocoa; offscreen does not drain
Apple's main dispatch queue. Headless Linux requires a disposable D-Bus/Secret
Service session.

The GitHub workflow defines macOS, Windows, and Linux tests, including a Linux
test keyring. Adding it does not establish that remote runs passed. Windows/Linux
runtime checks must pass on those hosts before claiming platform validation.
