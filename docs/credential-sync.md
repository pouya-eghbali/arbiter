# Local credential sync

Desktop uses Qt Network TLS and QtKeychain; mobile uses the same encrypted record
format with native credential providers. Sync stays LAN-only: foreground polling
and optional OS-scheduled phone background jobs, with no relay or cloud backup. Passphrases, OS unlock credentials and device biometric data never sync.

## Trust and encryption

Pairing is explicitly initiated in desktop Settings. A two-minute single-use QR
contains the local HTTPS endpoint, SHA-256 certificate pin, vault ID, random pairing
code and a transport encryption key. Anyone who obtains an unexpired invitation
can pair: treat it as a secret. The phone confirms the desktop identity before
sending the code. It accepts only local/private endpoints, the exact certificate
pin and no redirects. The desktop only accepts local IPv4 peers, bounded POST
requests, no browser Origin header, and bearer authentication. Its certificate is
self-signed; trust comes from the out-of-band QR, not ambient certificate roots.

Each device receives a random revocable bearer token. TLS protects the exchange;
AES-256-GCM also authenticates record envelopes with a fresh 12-byte nonce, 16-byte
tag and `ArbiterSync/v1/<vaultId>` additional authenticated data. Maximum payload
size is 16 MiB. At-rest keys are distinct from the wire key. The desktop's separate
master key and TLS private key are stored through QtKeychain. Mobile stores its
local AES key in the OS credential facility (iOS device-only unlocked Keychain;
Android FlutterSecureStorage). Credential files and pairing tokens are encrypted.
A revoked peer's knowledge of the wire key does not expose the desktop state file.
Previously received credentials cannot be remotely erased.

The desktop requires passphrase/biometric vault verification before serving and
locks on OS session lock or after ten minutes by default. An explicit desktop
setting can keep a sync session alive until computer/manual lock or application
exit so that phone background jobs can reach it. That opt-in never bypasses the
initial passphrase/biometric verification or an OS session lock. Mobile's foreground sync may access
the local encryption key while the credential list is locked, following explicit
pairing consent. Listing/filling credentials and signing passkey challenges require
fresh native device authentication. This is an application verification boundary,
not protection against a compromised OS or a modified Arbiter executable.

## Records and conflicts

Records use a multi-value register with per-device vector clocks. Dominated values
are removed; concurrent edits and edit/delete conflicts remain available for explicit
resolution on the phone. Deletions use tombstones. All clients merge the most recent
on-disk state under a process-shared file lock before atomically replacing encrypted
state. On iOS, a native transaction gate additionally serializes the foreground and
headless Flutter engines, since POSIX record locks alone do not isolate engines in
the same process. Background cancellation waits for an active transaction to release
before destroying its engine. Android runs scheduled sync in a separate process. This
prevents UI, background sync and provider processes from losing updates.

Password identity is SHA-256 of canonical origin, NUL, username. Native password
stores enforce HTTPS origin and account binding. Passkeys retain RP ID, credential
ID, user handle, P-256 private key (portable PKCS#8), COSE public key and backup flags.
Only ES256 is supported. Each receiving desktop reseals keys under its own local
vault key; the desktop passphrase is never transferred. Legacy non-backup-eligible
credentials stay on their original desktop and are excluded from transfer. Re-register
a passkey to create a sync-eligible replacement; its original eligibility cannot change. New eligible credentials are marked backed
up after an actual peer copy is observed. Authenticators use a zero signature counter.

No JavaScript WebAuthn replacement is installed. Desktop uses its patched Chromium
provider, iOS uses AuthenticationServices, and Android uses Credential Manager via
GeckoView. The OS/browser verifies the RP/origin and challenge; native providers
verify the selected credential and perform device authentication before signing.

## Test and release boundaries

See [mobile/README.md](../mobile/README.md) for reproducible tests and provisioning.
Crypto vectors, cross-language key imports, concurrent storage, TLS pinning and real
native-peer transfer tests use fabricated credentials only. Physical-device system
picker and website interoperability tests remain a release gate. iOS distribution
also requires Apple's managed browser entitlement approval. This implementation has
not had an independent security audit; do not describe it as audited.
