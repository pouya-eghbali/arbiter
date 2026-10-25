# Media codecs and Widevine: developer guide

Arbiter exposes H.264/AAC formats without bundling their software decoders.
Widevine is discovered from an existing installation; Arbiter neither distributes
nor downloads the CDM. This guide describes the patched engine, not stock Qt.

## Site muting

The pill always shows a speaker button on HTTP(S) pages. Click it to mute/unmute
the site, even before it starts playing audio. The button changes to a highlighted
crossed-out speaker when muted. **Mute site / Unmute site** is also available in
the page's right-click menu; More does not duplicate the pill control.

The rule covers all open and future tabs with the exact same hostname, across
normal workspaces and ports. Subdomains remain separate. Normal choices are saved
in `Audio.mutedSites` in `settings.toml`. Private windows begin with those choices,
but edits stay in that private window's memory and disappear when it closes.
Private-window locking always mutes the page regardless of the site's rule.

## Decoder paths

| Platform | H.264 | AAC | Bundled FFmpeg |
| --- | --- | --- | --- |
| macOS | VideoToolbox | AudioToolbox | Chromium configuration; no H.264/AAC decoders |
| Windows | Chromium's native Windows video path | Media Foundation | Chromium configuration; no H.264/AAC decoders |
| Linux | System FFmpeg / available platform paths | System FFmpeg | None; external shared libraries required |

The Windows native AAC adapter accepts ordinary AAC as well as the existing
xHE-AAC path. macOS selects the appropriate AudioToolbox format and only sets
xHE-specific loudness/DRC properties for xHE-AAC.

For MSE, Chromium prepends an ADTS header to ordinary AAC access units. Native
decoders expect raw packets. `arbiter_aac_packet.h` removes the header **after
CDM decryption**, so encrypted subsample offsets remain valid. Raw packets pass
through unchanged; truncated headers, mismatched frame lengths, and multiple
raw data blocks are rejected. Both native AAC adapters use this helper.

## Build configuration

Use the [patched SDK build instructions](passkey-integration.md#build).
`engine/build.py` sets `FEATURE_webengine_proprietary_codecs=ON` to expose the
formats. Our patch changes what that flag builds:

- macOS/Windows: `ffmpeg_branding="Chromium"`, omitting the H.264/AAC decoders.
- Linux: `FEATURE_webengine_system_ffmpeg=ON` is required. Configuration must
  fail rather than silently fall back to bundled codec implementations.
- All platforms: `media_use_openh264=false` and `rtc_use_h264=false`. H.264
  WebRTC is disabled; other supported WebRTC codecs remain available.

Do not replace these settings with stock Qt's proprietary-codec configuration:
stock Qt selects the Chrome FFmpeg configuration, which includes the decoders.
An application-only build cannot add missing engine format support.

The build driver audits the generated configurations before installing the SDK:

```sh
python engine/media_policy.py --source /path/to/work/source --build /path/to/work/build
python -m unittest discover -s engine -p test_media_policy.py
```

The audit checks format negotiation, OpenH264/WebRTC flags, external FFmpeg on
Linux, and disabled/unregistered AAC/H.264 decoders in the selected native-build
FFmpeg configuration. It is a build check, not a complete legal or package audit.
Deployment tools must not copy system FFmpeg libraries or Widevine into a bundle.

For this development machine, use `--jobs 24` with the engine driver and
`--parallel 24` with application/test builds. Size concurrency for available RAM
on other machines; the driver's job count also limits Qt's nested Ninja process.

## CDM discovery

Qt discovers Widevine at engine startup in supported browser/distro locations,
including Chrome's standard installation. Restart Arbiter after installing or
updating Chrome. The library must match the engine's CPU architecture and CDM
interface. Detection does not guarantee streaming-service authorization or HD/4K.

For diagnostics, an explicit library can be selected before launching:

```sh
QTWEBENGINE_CHROMIUM_FLAGS="--widevine-path=/absolute/path/to/library" /path/to/arbiter
```

The filenames are `libwidevinecdm.dylib` (macOS), `widevinecdm.dll` (Windows),
and `libwidevinecdm.so` (Linux). Leave the override unset when validating automatic
discovery. Avoid copying a CDM into the project or SDK to make a test pass.

## Verification and troubleshooting

Follow the [manual media probe instructions](../tools/media-probe/README.md).
The probe uses a temporary profile and public Shaka media/license services.
It verifies format support, EME negotiation, and actual encrypted H.264/AAC
playback with advancing time and decoded frames. Network tests are manual;
the offline AAC packet test is registered as `aacpacket` in CTest.

```sh
ctest --test-dir /path/to/test-build -R '^(aacpacket|passkeyengine|passkeycontroller)$' --output-on-failure
```

- Missing H.264/AAC support: check the selected SDK and helper first. Qt can
  prefer stock WebEngine packages even when the SDK prefix appears first.
  Use `ARBITER_PATCHED_WEBENGINE=ON` and `ARBITER_WEBENGINE_PREFIX` explicitly;
  the probe uses the same runtime configuration helper as the app.
- EME negotiation fails: check CDM discovery, architecture, interface
  compatibility, and codec support separately.
- Native AAC decode fails: check raw access-unit framing, including ADTS removal
  after decryption. Do not adjust encrypted subsample offsets in this adapter.
- Playback works in the probe but fails on a service: investigate that service's
  browser/CDM requirements. A demo license does not validate production access.

On 2026-10-24, the macOS arm64 probe passed using automatic Chrome CDM discovery:
H.264 Main + AAC-LC, 84 decoded video frames over more than three seconds,
zero corrupted frames. Symbol inspection of the installed engine found native
AudioToolbox/VideoToolbox references and no AAC/H.264 decoder or OpenH264 encoder
registration symbols. The app build/signature, six packaging-policy tests, AAC
packet test, and two passkey regression suites passed. Windows/Linux runtime
playback remains unverified on this machine.

There is no bundled H.264/AAC software fallback. Systems missing suitable
decoders cannot play these formats. Standalone raw AAC/ADTS playback is outside
this integration's validated scope. Using external/native libraries does not
itself establish patent-license coverage; review distribution obligations
separately.
