# Local credential sync

Desktop uses Qt Network TLS and QtKeychain; mobile uses the same encrypted record
format with native credential providers. Sync stays LAN-only: foreground polling
and optional OS-scheduled phone background jobs, with no relay or cloud backup. Passphrases, OS unlock credentials and device biometric data never sync.

## Multiple computers and phones

Connections form a trusted replication graph: each connected device exchanges
its complete versioned vault, so changes can travel through a shared peer. Each
connection stores its pinned endpoint and individual bearer token. Transport
encryption keys belong to the serving desktop and are separate from at-rest keys. Linking another computer explicitly shares the combined vault with the
devices that computer already syncs with; confirm that choice before joining.
The graph is one shared credential set, not separate per-computer password lists.
Device names also travel with the versioned state so conflicts remain readable
when changes arrive through a shared peer. This directory is display metadata,
not an authorization or membership list; the device page lists direct connections.

On computer A, unlock Sync and choose **Add a device**. For another computer,
copy the pairing code and paste it into computer B's Sync settings, review the
device and certificate fingerprint, then connect. On a phone, scan A's code;
**Add another computer** adds B without replacing A. Existing single-computer
phone pairings upgrade automatically. A phone can bridge two computers without
those computers being paired directly; direct computer-to-computer pairing also
works without a phone. Phones initiate connections rather than hosting a LAN
server. Devices need a connected path over time, not simultaneous availability
of every device. No internet relay or automatic membership/key sharing is used.

Desktop polls outgoing connections while sync is unlocked; mobile polls in the
foreground and can use OS-scheduled background jobs. A failed/offline connection
does not stop the other connections in a batch. The device list shows the last
successful exchange and whether local state is waiting to be acknowledged.
These timestamps are display information only, never conflict precedence.
Changed LAN addresses can be refreshed by pairing again with the new invitation.

Removing an incoming device revokes its bearer token on that computer. Removing
an outgoing connection forgets its endpoint/token locally. Neither operation
deletes credentials or removes other paths in the graph. To stop a lost device
that has several connections, revoke each of its connections on the remaining
computers; there is no global remote wipe. Phones support 16 computer connections; desktops support 16 incoming and 16
outgoing connections. Credential histories support up to 32 device identities.

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
Android Keystore wrapping). Credential files and pairing tokens are encrypted.
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
resolution on desktop or phone. A resolution is itself a versioned edit and
propagates to the other devices. Choices carry a generation digest: a stale
screen cannot silently resolve additional changes that arrived since it was
displayed. Identical independent password imports coalesce with their causal
histories instead of creating a conflict. Different values are never discarded
using wall-clock timestamps.

Deletions use tombstones, retained indefinitely rather than aged out while an
offline device may still hold an old copy. A stale copy cannot resurrect a
causally newer deletion; a concurrent edit/delete needs an explicit choice.
Disconnecting a device leaves its local credentials intact. All clients merge the most recent
on-disk state under a process-shared file lock before atomically replacing encrypted
state. The Qt mobile client additionally serializes transactions with an in-process
mutex, since POSIX record locks alone do not isolate threads in the same process.
Cancellation invalidates pending network replies before discarding a worker.
Android runs scheduled sync in a separate process. This
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
Android System WebView. The OS/browser verifies the RP/origin and challenge; native providers
verify the selected credential and perform device authentication before signing.

## Test and release boundaries

See [mobile/README.md](../mobile/README.md) for reproducible tests and provisioning.
Crypto vectors, cross-language key imports, concurrent storage, TLS pinning and real
native-peer transfer tests use fabricated credentials only. Physical-device system
picker and website interoperability tests remain a release gate. iOS distribution
also requires Apple's managed browser entitlement approval. This implementation has
not had an independent security audit; do not describe it as audited.

On macOS, background credential exports use noninteractive Keychain reads. If a
legacy item needs approval for this app, sync stops with a permission error rather
than opening a chain of Keychain password dialogs. Explicit password filling may
still ask macOS to authorize that individual item; “Always Allow” applies to that
item, not every credential sharing the service label. Existing item ACLs are not
removed or broadened. The native adapter serializes credential-store operations
and restores the process's previous interaction policy after each silent read.
