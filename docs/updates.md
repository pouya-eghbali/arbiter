# Release checks

Settings → About shows update status, the last successful check, a manual check
button, and a link to the published release. Automatic checks are enabled by
default and can be disabled there or with `[Updates] automatic = false` in the
TOML preferences. Manual checks remain available when automatic checks are off.

After the regular browser window starts, Arbiter waits ten seconds before checking
whether a check is due. It checks at most once every 24 hours, including across
restarts; an hourly timer also handles long-running sessions. Failed automatic
checks wait until the next day rather than repeatedly retrying. Nothing checks
during onboarding or the release smoke test. Private windows do not schedule
additional checks.

The separate network client requests the public GitHub API endpoint
`https://api.github.com/repos/pouya-eghbali/arbiter/releases/latest`. The request
uses a fixed `Arbiter-Update-Check` user agent and sends no browsing history,
profile cookies, authentication credentials, installed version, or installation
identifier. GitHub receives the normal network request, including the user's IP
address. No analytics or telemetry are added.

Only published stable semantic versions are accepted. The tracker compares
versions numerically, accounts for prerelease ordering in development builds,
and never offers a downgrade as an update. Cached release information and check
times are stored under `[Updates]`. Replies are bounded to 256 KiB and a
20-second total deadline; redirects and malformed, draft, or prerelease metadata
are rejected. Release links are constructed from the validated tag under the
Arbiter GitHub repository, rather than trusting response-provided links.

The tracker opens release notes/downloads for the user to review. It does not
download packages, replace binaries, install updates, or restart the app.
Future Windows assets can be added to a release without changing the checker.

`updatetracker_test` uses a local HTTP fixture to cover version comparisons,
stable-release validation, error recovery, daily scheduling, persisted opt-out,
and cookie/authentication-free requests. It does not contact GitHub.
