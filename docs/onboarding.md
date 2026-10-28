# Onboarding

Arbiter opens a separate setup window until `onboarding.version = 1` is saved
in the XDG TOML settings. Existing installations without that marker see setup
once too. The browser's WebEngine views are constructed after setup, so choosing
Classic applies native window decorations immediately, without restarting.

The flow is Welcome → Import → Preset → Colors → Default browser → Ready.
The Ready screen and final tutorial step show the current shortcut-help binding
(Command+/ on macOS, Control+/ elsewhere by default). This opens a compact,
scrollable sheet of common actions using the user's actual bindings. Its
See all shortcuts button opens Settings → Keyboard, including when invoked from
a private window (preferences open in the regular window). The help shortcut is
customizable too. If an existing installation already assigned Command/Control+/
to another action, help uses an available function key to preserve that binding.

Back is available throughout. Closing setup preserves the current step and any
appearance changes or completed imports. The browser/profile scan starts on the
Import screen. Browser cards show installed-app icons (or an initial badge), the
available data, and a profile count in a responsive two- or three-column grid.
On macOS 27 and later, installed browsers are listed using Launch Services app
identifiers before reading protected browser profiles. A browser is not hidden
just because its data is inaccessible. Selecting an access-required card opens
an AppKit NSOpenPanel sheet at that browser's specific data directory (for
example, `~/Library/Application Support/Google/Chrome`), without asking the user
to find it. Access to the parent Application Support directory is insufficient.
The returned URL is validated against the requested directory, and its security
scope is retained for the importer lifetime. Approval rescans the target and is
reported successful only if importable profiles are actually found there.
Cancellation or a failed readability check leaves the browser listed and
unselected, preserves other choices, and imports nothing. Settings → Import
lists the same browsers before access; choose one and use Allow <browser> access.
After approval, the first readable profile is selected automatically. The app
supplies NSAppDataUsageDescription for macOS's own data-access prompts too.
Native sheet presentation at Chrome's own folder and cancellation were verified
on macOS. Successful profile discovery, rejection of an approval that leaves data
unreadable, selection preservation, and no credential reads during authorization
are covered using synthetic profiles. Real access approval is left to the user.
Select multiple browsers; Import and continue
imports every detected profile in those browsers, one at a time. Password unlock
prompts are serialized. A shared import status component shows an animated spinner
and the reading/saving stage in setup and Settings. Saving shows a progress bar
with completed password writes; an in-flight write does not count as completed.
The setup status appears above the browser cards and is brought into view when
importing starts. Success advances to presets; failures stay on the Import
screen with per-profile results, Retry failed imports, and Continue anyway. Start
fresh skips import. The underlying OS authorization, duplicate handling, and
storage rules are the same as Settings → Import. Back/Continue are
unavailable while an import or native default-browser request is in progress.

Minimal hides both the navigation pill and decorations. Glacier shows the pill
and hides decorations; it is the fresh-install default. Classic shows both.
Existing appearance choices are preserved until the user changes them. Colors
includes both accent and background swatches/pickers for the currently displayed
Light or Dark appearance. The two sets of colors are saved independently. The
welcome reveal finishes in under a second. Reduced motion is initially taken
from macOS/Windows accessibility settings or the Qt platform style's animation
hint on Linux, and can also be enabled on the Welcome screen.

Default-browser changes happen only when Set as default is pressed:

- macOS uses [NSWorkspace's public API](https://developer.apple.com/documentation/appkit/nsworkspace/setdefaultapplication(at:toopenurlswithscheme:completion:))
  for HTTP and HTTPS, including the system consent prompt. Launch the `.app`
  bundle; a standalone development executable cannot become the default.
- Windows registers per-user browser capabilities and opens
  [Default Apps](https://learn.microsoft.com/en-us/windows/apps/develop/launch/launch-default-apps-settings)
  for Arbiter. The user makes the final choice there. No UserChoice hashes or
  protected associations are edited.
- Linux writes `com.arbiter.browser.desktop` under `$XDG_DATA_HOME/applications`
  (default `~/.local/share/applications`) using the running executable, then runs
  `xdg-settings set default-web-browser`. Missing tools or desktop failures show
  an explanation and leave the optional step skippable. The executable must
  remain at that path for external links to work.

Default status is checked after the request and when returning from system UI.
Setup does not advance automatically or treat launching settings as success.
Native Windows/Linux default-browser operations still need validation on those
systems; automated UI checks skip changes to the developer's actual defaults.

External HTTP(S) arguments are handled on Windows/Linux as well as macOS's
native open-URL events. A profile lock and a user-restricted local socket forward
links and activation requests to a running Arbiter instead of opening its
persistent profile in a second process. URLs received during setup are queued
until the browser window is ready.

Ready offers Show me around or Start browsing. The tour runs in the actual
browser and covers addresses, tabs/workspaces, and split views, using current
customized shortcuts. Its navigation explanation adapts to a hidden pill.
Try buttons perform the corresponding browser action; they don't open a sample
website. Skip/finish dismiss the tour. Settings → Configuration → Replay welcome
tutorial starts it again. An unfinished tour resumes after restarting.

Passkeys are not imported by the browser-profile or CSV importer. Both setup and
Settings explain that they stay with the current provider. Secure passkey
migration is possible through participating credential providers, but Arbiter
has no credential-exchange integration yet. Apple's
[credential import API](https://developer.apple.com/documentation/authenticationservices/ascredentialimportmanager)
requires a credential-provider extension and an exchange initiated by another
participating app. This is a separate integration from decrypting browser password
databases. Chrome's [desktop CSV flow](https://support.google.com/chrome/answer/13068232)
imports passwords; it does not supply portable passkey private keys to this importer.

The controller tests cover progress, all presets, interrupted setup, completion,
tutorial persistence, invalid settings, and actual shared-button foreground colors.
Importer tests cover multiple browsers/profiles, sequential processing, partial
failure, retrying only failures, duplicate handling, and cancellation without
starting the remaining profiles. The browser-instance test exercises
HTTP(S) forwarding, activation, ignored non-web arguments, and profile reopening.
Headless UI verification uses an encrypted synthetic browser vault and checks
all six screens, preset/theme switching, skip/import paths, native-window flags
at first browser launch, and tutorial completion/replay without touching real
passwords or changing the default browser.
