# Manual media smoke test

To check actual H.264 video and AAC audio decoding without DRM or external
services, supply a local MP4 fixture containing those codecs:

```sh
./build-media-probe/arbiter-media-probe --media-file /path/to/h264-aac.mp4
```

This mode serves only the fixture on loopback, uses an off-the-record profile,
and stays muted. Success requires at least three seconds of advancing video,
15 decoded video frames, and successful independent AAC-to-PCM decoding. It
does not read your browser profile or require a Widevine module.

Without `--media-file`, the existing network/DRM probe below runs.

This standalone probe uses an off-the-record Qt WebEngine profile, without
reading Arbiter profiles or accounts. It contacts the public Shaka demo player,
Google-hosted demo media, and Shaka's demonstration Widevine license server.
It opens a small playback window and closes it when the test completes.
It does not download or install a CDM. Run it only when network testing is wanted;
it is deliberately excluded from ordinary unit tests.

Configure with the same explicit SDK selection as the app. Qt otherwise prefers
WebEngine modules from beside its base package, even with the SDK prefix first:

```sh
cmake -S tools/media-probe -B build-media-probe -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/base/Qt" \
  -DARBITER_PATCHED_WEBENGINE=ON \
  -DARBITER_WEBENGINE_PREFIX="/path/to/patched/sdk"
cmake --build build-media-probe
./build-media-probe/arbiter-aac-packet-test
./build-media-probe/arbiter-media-probe
```

On Windows use the executable under the corresponding build/configuration
directory. For a single-architecture macOS SDK also pass
`-DCMAKE_OSX_ARCHITECTURES=arm64 -DQT_IS_MACOS_UNIVERSAL=OFF` (or `x86_64`).
Qt's normal Widevine discovery is used; leave `QTWEBENGINE_CHROMIUM_FLAGS`
unset to test automatic discovery. An optional explicit `--widevine-path`
can diagnose discovery separately from playback.

Exit 0 / `PROBE_PASS` requires H.264/AAC HTML media and MSE support, successful
Widevine key-system negotiation, an active H.264/AAC track, and at least three
seconds of advancing encrypted playback with at least 15 decoded video frames. The probe
is muted. Exit 1 / `PROBE_FAIL` reports errors or a 90-second timeout; exit 2
means the HTML fixture could not be read. A result depends on test-service
availability, network, compatible CDM installation, and platform support.
Passing this test does not establish Netflix/Prime authorization, output quality,
hardware decoding, or codec licensing rights. Use Chromium's media diagnostics
to inspect the selected decoder separately.
