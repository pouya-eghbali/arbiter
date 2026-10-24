# Downloads

`Downloads` listens once to `QQuickWebEngineProfile::downloadRequested` for every
persistent workspace profile and each private window's separate profiles. It uses
Chromium's download requests, preserving the originating profile's cookies,
authentication, blob URLs, and referrers. It does not refetch files with an unrelated
network client. Closing a tab does not cancel a running transfer.

The default destination is `QStandardPaths::DownloadLocation`. Settings → Downloads
lets users choose an existing folder, persisted as `downloads.directory` in the
portable TOML preferences. Subsequent downloads across profiles and private windows
use that setting. There is no Save As prompt in this implementation.

For a new network download the app sanitizes the suggested filename for macOS,
Windows, and Linux; bounds its UTF-8 length; handles Windows reserved names; and
reserves a numbered alternative with `QFile::NewOnly`. Existing files are not
replaced. An empty placeholder reserves the destination until Chromium
renames the completed transfer into place. Empty reservations are removed on
cancellation or terminal failure. Qt page saves already in progress retain their
explicit destination rather than trying to change an accepted request.

## Controls and lifecycle

The pill's More menu shows Downloads and an active count. Cmd+Shift+J on macOS or
Ctrl+J on Windows/Linux opens the same panel even when the pill is disabled;
Keyboard settings can reassign it. Starting a download opens the panel when the
window is not in Settings, locked, or showing a sensitive permission flow.

- Progress reports received/total bytes; unknown sizes remain labeled by received
  bytes. Pause and resume apply to active Chromium transfers.
- Cancel stops an active request. Retry appears for interrupted requests Chromium
  reports as resumable and resumes that same request, including HTTP Range support
  and its authenticated profile. Cancelled or terminal failures must be downloaded
  again from the source page.
- Open is explicit and only available for completed files. Missing files produce a
  visible error. Files never launch automatically.
- Show in folder selects the file in Finder/Explorer when available; Linux opens
  the containing directory through the desktop's registered file manager.
- Clear finished removes inactive list entries, including interrupted requests and
  their remaining temporary data, but never deletes completed files.
- Closing the app or a private window cancels its running transfers. Completed
  history is preserved for regular downloads. Previous incomplete history is
  marked interrupted after restarting; transfers do not auto-resume across restarts.

Regular history is atomically saved to `downloads.json` under the app data directory
with owner read/write permissions, retaining up to 500 recent entries. Embedded URL
credentials and fragments are excluded; data/blob payload URLs are not persisted.
History contains local file paths and source URLs and is separate from portable
preferences. Entries do not retain web views, and request pointers are guarded.

Each private window has an independent in-memory download manager. It never writes
a download history file and never adds rows to the regular list. Locking hides the
panel and rejects new download requests until the window is unlocked; existing
transfers can finish. Private-window closure cancels transfers and drops the list.
Downloaded files and OS metadata still remain on disk.

Completed files receive macOS quarantine metadata or Windows Zone.Identifier
Internet-zone metadata. Metadata failures are visible on the entry. Linux has no
universal equivalent implemented here. This is OS integration, not a malware scan
or Safe Browsing verdict; files remain unopened until the user chooses Open.

## Validation

```sh
cmake --build build-patched-tests --target downloads_test keybindings_test --parallel 24
ctest --test-dir build-patched-tests -R '^(downloads|keybindings)$' --output-on-failure
```

`downloads_test` uses temporary folders and a local HTTP server with real Qt Quick
WebEngine views. It covers filename sanitization/reservation, byte-for-byte completed
files, duplicate-name preservation, progress, pause/resume/cancel, an interrupted
transfer resumed with HTTP Range, already-running page saves, history restoration,
blocked requests, private-profile isolation, and clearing history without removing
saved files. On macOS it also verifies the quarantine attribute is present.
Runtime validation has been performed on macOS; Windows/Linux runtime validation
remains pending. Open/reveal are desktop integration actions and are not launched
by the automated transfer test.
