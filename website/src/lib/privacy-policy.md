# Arbiter privacy policy

Last updated: 1 November 2026

Arbiter is an open-source browser. We do not operate an analytics, advertising,
telemetry, crash-reporting or browsing-data collection service in Arbiter. This
policy covers the browser and its optional credential and local-sync features.
Websites and external services you choose to contact have their own policies.

## Browsing data stays on your device

Regular browsing can save your tabs, workspaces, settings, history, website cookies,
cache and other site storage locally. Downloads are saved to your device. Page
previews used for tabs and floating panels are generated locally. Private browsing
avoids saving regular history and uses separate website storage. It does not hide
your traffic from websites, your network operator or your internet provider.

On iOS, private tabs use nonpersistent WebKit storage. Android uses temporary
WebView profiles with cookies and persistent DOM storage disabled. Their site data
is cleared when the last view closes, and remaining profile shells are removed at
next startup. This depends on Android System WebView support and is not a promise
of identical behavior to another browser's incognito mode. Downloaded files and
credentials you explicitly save can outlive a private session.

## Passwords, passkeys and device authentication

Saved credentials are encrypted on your device. iOS protects the vault key with
Keychain; Android uses Android Keystore to protect the vault key. When enabled,
the credential provider shares the encrypted vault with Arbiter's provider on the
same device so you can fill credentials in other apps. Android's system credential
provider currently supports passkeys, not system-wide password autofill.

Biometric checks and device passcodes are handled by the operating system. Arbiter
does not receive your fingerprint, facial biometric data or device passcode.
Website passwords are different: the browser must access a selected saved password
to fill it into the website you choose. Only approve filling into a trusted site.

## Optional local encrypted sync

Paired devices exchange encrypted credential records directly on your local
network. Pairing uses device identifiers, connection addresses and certificate
fingerprints to identify and authenticate peers. We do not provide a cloud vault,
hosted relay or remote backup service. Background sync is optional and depends on
operating-system scheduling and device lock state.

Credential changes, conflicts and deletion markers are exchanged with paired
peers. Deletion markers are retained so another device cannot silently restore a
deleted record. An offline device receives a deletion only after it reconnects.
Disconnecting a peer stops that connection; it does not remotely erase copies
already held by that device. Remove unwanted records on your remaining devices
and allow deletions to sync before disconnecting if you want them removed there.

## Permissions are optional

A website may request your camera, microphone or location. Arbiter asks for consent
and uses operating-system permission controls; you can deny requests or revoke
access in device settings. Location may be approximate where the operating system
supports that choice. Camera access also supports scanning a local pairing code.
Local-network access is used to connect to paired devices. File selection and
sharing send the files or URL you choose to the website, app or recipient you
select. The browser does not upload camera, microphone or location data to an
Arbiter collection service.

## Connections to other services

Browsing sends requests to the websites you visit and their resources. Searches
are sent to your selected search engine. Websites and search engines receive your
IP address and information carried by those requests. Website icons and embedded
content can contact their hosting services. The operating system or network may
provide DNS resolution, location services and WebView updates under their own
policies.

The desktop app can check GitHub for releases; automatic checks can be disabled
in its settings. Opening GitHub, reporting an issue, downloading software or
visiting our website contacts the service concerned. Never include passwords,
passkeys or private browsing data in a public issue. Our public website is hosted
by Vercel and uses Cloudflare DNS; those providers process connection information
under their respective policies. The website does not include analytics trackers.

## Retention and deletion

Local data remains until you delete it, clear website storage using available
browser or system controls, or remove the app's data. You can delete history and
saved credentials in Arbiter and disconnect sync peers in settings. Deleting a
history entry does not clear that website's cookies or downloaded files. Removing
an app may leave downloaded files, exported backups, credentials in an OS vault,
or copies on paired devices; delete those separately where necessary. Backup and
restore behavior also depends on your operating system. Android app backup is
disabled; iOS vault keys use device-only Keychain protection.

## Contact and changes

The project is maintained at https://github.com/pouya-eghbali/arbiter. For privacy
questions, use the maintainer contact options on
https://github.com/pouya-eghbali or open an issue without including sensitive data.
We will update this policy when the browser's data practices change and publish
the revised date here. The policy source and its revision history are public.
