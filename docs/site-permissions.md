# Website permissions and location

Arbiter handles Qt WebEngine's `permissionRequested` signal using
`QWebEnginePermission`. Requests are attached to their originating tab, queued,
and displayed only while that tab is focused and its window can accept input.
The Glacier prompt displays the requesting origin (scheme, host, and port),
plus the top-level page origin when a frame requests access. Origins are rendered
as plain text without paths, credentials, query strings, or fragments.

Allow/Block apply to the requesting origin in its Chromium profile. Qt remembers
location, notification, clipboard, and local-font decisions. Camera, microphone,
screen capture, and pointer lock remain temporary according to Qt's API.
Escape/dismissal, navigation, tab destruction, shutdown, and private-window
locking discard pending requests without creating a saved site block. A stale
button press cannot resolve a different queued request. Background tabs wait
until focused; the queue is bounded.

Screen sharing additionally uses `desktopMediaRequested`. The source picker
requires an explicit screen/window selection, supports scrolling, and cancels
on navigation, tab changes, private locking, and shutdown. It never selects a
screen automatically. Platform capture/portal support and OS approval are still
required; granting a website permission cannot bypass those controls.

## Privacy preferences

Settings → Privacy includes **Allow sites to request your location**, enabled
by default. Enabling it permits prompts; it does not grant a site access.

```toml
[Privacy]
geolocationEnabled = true
```

Disabling it rejects pending/new location requests and resets saved location
grants in every open normal/private profile. Newly opened profiles have their
old grants reset before they are used. Turning it back on requires fresh site
consent. Existing explicit site blocks remain blocked.

The same page lists saved decisions for normal profiles opened in the current
session, with a Reset action to return a permission to Ask. Decisions in private
windows use Qt's memory-only storage, are excluded from this list, and disappear
when their private profile is destroyed. Permissions are stored in Chromium
profiles, rather than in the shareable TOML settings file.

## Geolocation backend and packaging

The engine build driver explicitly requires `FEATURE_webengine_geolocation=ON`.
The app requires Qt Positioning. Deploy its native runtime and position plugins
alongside WebEngine's normal resources:

- macOS uses Qt's Core Location backend. The signed app bundle includes location,
  camera, and microphone usage descriptions. The OS may ask for application
  consent after the user grants website consent.
- Windows requires a suitable Qt Positioning backend and enabled Windows
  location services, including desktop-app access.
- Linux requires a usable positioning backend such as GeoClue or GPS, its
  runtime service, and any applicable system/desktop permissions. Arbiter does
  not introduce an IP-location service, API key, or invented location fallback.
  The app supplies `com.arbiter.browser` as its desktop ID and installs the matching
  desktop entry for GeoClue. Development/unpacked distributions must make that
  metadata available too. The entry assumes `arbiter` is on the executable PATH.

If the OS denies access or no provider can supply a position, granting the site
permission alone cannot produce coordinates. Keep browser and OS consent
separate when diagnosing errors.

The engine patch also fixes an asynchronous accuracy-variable lifetime issue
in Qt's location helper and reports denied OS consent as a permission error.
Permission-status updates use the requesting origin, so resetting an iframe's
grant notifies that frame as well as top-level documents.

## Tests

```sh
cmake --build build-patched-tests --target sitepermissions_test --parallel 24
ctest --test-dir build-patched-tests -R '^sitepermissions$' --output-on-failure
cmake --build build-patched-app --target arbiter arbiter_qmllint --parallel 24
```

The tests use temporary/off-the-record profiles and loopback pages. They cover
origin sanitization, focus/queue routing, stale decisions, cancellation,
shared-origin decisions across tabs, global revocation, preference persistence,
saved-decision reset, private isolation, and actual browser rejection while
location is disabled. They do not retrieve the user's real location or activate
physical capture devices. Actual positioning and capture availability must be
validated separately on each target platform.

Permission consent is distinct from implementing a notification presenter,
background push service, or another feature that consumes a granted permission.

References: [Qt permissions](https://doc.qt.io/qt-6/qwebenginepermission.html),
[Qt WebEngine features](https://doc.qt.io/qt-6/qtwebengine-features.html#html5-geolocation).
