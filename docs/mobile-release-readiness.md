# Mobile release-readiness audit

## Remediation and current distribution scope — 1 November 2026

The project now targets **self-built Android and iOS installations only**, as
requested. No mobile APK or IPA is to be published in releases. App Store/Play submission, managed default-browser
approval and store-console requirements are deferred. No GPL exception or
commercial Qt purchase has been made. The original audit below is retained as
historical evidence; its original blocker descriptions are not a claim that the
same bugs remain after these changes.

| Finding | Current status |
| --- | --- |
| R1 privacy manifests | App and credential-provider manifests packaged; app declarations merge the linked static Qt framework manifests. App-only/app-group preferences and file access are declared. SDK collection/tracking changes fail configuration for review. Verified generated bundles; this is not an Apple privacy-report certification. |
| R2 privacy policy | One policy source is bundled offline in mobile Settings → Privacy/About and rendered by the website `/privacy` route. Website source/build validated; deployment is separate and has not been performed in this change. |
| R3 Android media permissions | Only supported, consented and OS-approved resources are granted. Origin checks, request identity, cancellation, navigation/destruction and overlapping requests are handled. Unsupported resources are denied. |
| R4 location | Coarse/fine declared and requested together; approximate-only approval succeeds. Grants are not persisted by our WebView prompt. |
| R5 builds/versioning | Qt property names corrected. Both platforms use `VERSION.txt` plus a configurable build number (currently 3). App/provider versions match. Android local-build APK alignment/signature/checksum helper added. iOS custom identifiers/groups generate matching plists/entitlements; the managed browser entitlement and its unavailable UI are off by default. |
| R6 accessibility | QML fonts follow iOS Dynamic Type / Android font scale. Android icon hit areas are 48 dp; iOS keeps 44-point targets with small visible close icons. Text fields/buttons grow with text; the tab grid becomes one column for larger text. Largest iOS accessibility size + Increase Contrast inspected. Full VoiceOver/TalkBack/device matrix remains unverified. |
| R7 native smoke | Workspace-editor dismissal is checked on the next bounded normal UIKit tick rather than inside a nested wait. The full native interaction smoke passes. Runtime diagnostics also exposed and fixed a text-field sizing reference and reentrant native-event/page-record updates. |

Validation for this remediation:

- iOS simulator build and signed device build succeed. No physical phone install
  or App Store submission was performed.
- Android Debug/Release APK builds succeed. Twelve Android 15 emulator
  instrumentation tests pass, including eight permission-coordinator regressions.
- APK signing helper tested end to end with a disposable key, with signature,
  16 KB ZIP alignment and checksum verification. The test key/APK were deleted;
  this does not establish a production signing identity or publish a release.
- Host mobile core passes; 55 mobile page renders pass without QML runtime errors,
  including offline privacy/license pages and small/landscape layouts.
- Native iOS interaction, keyboard/Reduce Motion and initial website appearance
  checks run with temporary profiles; no real credentials are used.
- Native Swift crypto vectors and three manifest/release-tool tests pass.
- Website type checking/build pass. The policy page has no analytics integration.

Remaining acceptance work includes physical-device credential-provider/biometric
flows, real OS permission dialogs/revocation (the coordinator tests use injected
OS responses), VoiceOver/TalkBack and Switch Control focus across native windows,
iPad/foldable behavior, IPv6-only networking, API 37 and a real 16 KB Android
runtime. The existing ELF/ZIP alignment checks do not replace runtime tests.
Unused Qt-injected Bluetooth and legacy external-storage permissions are removed
from the merged manifest; audio-settings permission is retained for media routing.
Review the merged permission list when dependencies change. Never call these
checks store approval.

The [mobile build guide](../mobile/README.md#build-your-own) documents signing,
installation, stable identifiers and source/relink distribution requirements.
Self-built full-feature iOS apps need an Apple team able to provision App Groups
and the credential provider; free Personal Team support is not promised.

### Follow-up: real Android browsing and pill interaction

Real Wikipedia browsing after commit `f489402` exposed a gap in the previous
Android checks: they tested native engine/profile integration and an unscrolled
cutout, not the actual Qt-window input path or page scrolling. Scrolling moved
the cutout, pill touches reached the wrong Qt window, and scrolled previews used
the wrong canvas origin. These have been corrected.

The new `PillInteractionTest` drives a real rendered page using injected system
touch events and screenshots. It checks fixed pill bounds/visibility after a
swipe, viewport preview pixels, opening Tabs through the pill and returning with
Back. Enabling accessibility for this test also exposed a synchronous
Android-UI/Qt-GUI thread deadlock during WebView creation. Creation is now
asynchronous, including safe cleanup if the page is closed before it completes.
The complete Android suite now passes 13 tests, including this regression.
Wikipedia, MDN and Hacker News were also opened through the actual app UI and
captured with real page previews. These checks supplement the earlier audit;
they do not establish full mobile
end-to-end or screen-reader coverage.

## Original audit (before remediation)

Reviewed 2026-11-01 against repository commit `052477c`, the existing Android APK,
the iOS device bundle, and a freshly rebuilt iOS simulator app.

**Verdict: neither mobile app is ready for store submission.** The engine choices
and native glass foundation are appropriate, but the confirmed issues below need
closing. This is a technical audit, not an App Store or Google Play approval.
No mobile release, store upload, licensing change, or production-account change
was performed as part of this audit. Application code was not changed.

## Confirmed release blockers

### R1 — iOS app and credential extension have no privacy manifests

Neither built bundle contains `PrivacyInfo.xcprivacy`. Our own code uses
UserDefaults in `mobile/platform/ios/NativeVault.swift:30–32`,
`CredentialVaultShared.swift:129`, and `mobileplatform_ios.mm:661,779`.
`mobile/CMakeLists.txt` does not package an application or extension manifest.

Add accurate manifests to **both** targets, including the standard-preferences
and app-group-preferences use cases. Audit the statically linked Qt modules and
their required-reason API declarations as well; a manifest covering only our
Swift code is not a complete dependency audit. Verify the final archive and its
privacy report, not just the source files. Apple requires declarations for
covered API use in the bundle containing that executable/library.
[Apple required-reason API rules](https://developer.apple.com/documentation/bundleresources/describing-use-of-required-reason-api).

### R2 — No accessible privacy policy

`mobile/qml/MobileSettings.qml:240–254` has About, GitHub, issue, and license links,
but no privacy-policy link. The website has no privacy route; an HTTP check of
`https://arbiterbrowser.com/privacy` returned **404**.

Publish a policy and link it in both apps and store listings. Explain local
history, credentials, device authentication, LAN pairing/sync, deletion and
retention, camera/microphone/location consent, downloads, and network requests.
The zero-telemetry statement is not a substitute for this policy. Complete the
store privacy forms from actual behavior, including dependencies; do not infer
the answers solely from whether we run a server. Open-web browsing is treated
separately from developer-controlled collection in both stores' guidance.
[Apple 5.1.1](https://developer.apple.com/app-store/review/guidelines/#privacy),
[Google User Data](https://support.google.com/googleplay/android-developer/answer/10144311),
[Google Data safety definitions](https://support.google.com/googleplay/android-developer/answer/10787469).

### R3 — Android grants an unfiltered WebView permission set

`mobile/platform/android/src/com/arbiter/mobile/NativeBridge.kt:152–153` maps
camera/audio resources to OS permissions, but line 179 subsequently grants
`p.resources` in full. A mixed request can therefore receive a resource for which
we never displayed a meaningful consent prompt or requested a corresponding OS
permission. Google's API documentation explicitly warns against granting the
entire requested resource array.

Grant only the supported, explicitly approved resources. Also bind pending
requests to their originating page/request, handle cancellation/navigation, and
avoid a single global pending slot being overwritten by another tab's request.
Add tests for mixed known/unknown resources, denial, partial grants, canceled
requests, and simultaneous requests.
[Android PermissionRequest](https://developer.android.com/reference/android/webkit/PermissionRequest).

### R4 — Android location requests omit approximate-location support

The manifest declares only `ACCESS_FINE_LOCATION`, and
`NativeBridge.kt:155` requests it alone. On modern Android this can be ignored on
some versions, and it does not correctly implement the approximate-location
choice. The shared result handler also treats any denied permission as complete
denial, rather than accepting an approximate grant where appropriate.

Declare coarse location, request coarse/fine together when precision is needed,
and evaluate the two results independently. Prefer approximate access when it is
sufficient. Verify both choices and settings revocation on-device.
[Android location permission guidance](https://developer.android.com/develop/sensors-and-location/location/permissions/runtime).

### R5 — Production artifacts and versioning are not established

- The inspected Android APK is **debug-signed**, with `versionName=1.0` and
  `versionCode=1`. `mobile/CMakeLists.txt:131` sets nonexistent
  `QT_ANDROID_APP_VERSION_NAME/CODE` properties; the installed Qt 6.12 macros
  consume `QT_ANDROID_VERSION_NAME/CODE` instead. Correct the properties and
  introduce a monotonic build number.
- iOS app and extension plists hard-code marketing version `0.2.0` and build `2`,
  independently of `VERSION.txt`. Generate consistent versions for both targets.
- `tools/mobile/build.py` compiles targets but does not establish a signed Play
  AAB/upload-key workflow or an App Store archive/export/validation workflow.
  A successful `--release` compilation is not a store-ready artifact.
- The existing iOS device build has `ARBITER_IOS_DEVICE_TESTING=ON`, which omits
  the managed default-browser entitlement. Verify Apple approval and production
  provisioning if default-browser selection will be offered. This entitlement
  is conditional on that feature, not a requirement for every WKWebView app.

New Google Play apps require an AAB and appropriate signing. Archive and validate
the iOS app and embedded provider with distribution profiles before submission.
[Android App Bundles](https://developer.android.com/guide/app-bundle),
[Play signing](https://developer.android.com/studio/publish/app-signing),
[Apple default-browser requirements](https://developer.apple.com/documentation/xcode/preparing-your-app-to-be-the-default-browser).

## UI and Liquid Glass findings

### R6 — Accessibility is incomplete

`MobileMain.qml:15`, `MobileField.qml`, `MobileButton.qml`, and many labels set
fixed pixel font sizes. Native Dynamic Type/Android font-scale adaptation has not
been demonstrated. Test the largest accessibility text sizes, wrapping and
scrolling, VoiceOver/TalkBack reading order, focus restoration across native
child windows, keyboard access, and labels for destructive actions.

`MobileIconButton.qml:6–7` and `MobileCloseButton.qml` use 44-point targets. This
matches Apple's normal target recommendation, but falls below Android's 48 dp
recommendation; some layout overrides are also 44 wide. Increase Android hit
areas without unnecessarily enlarging the visible icon face. These are design
and accessibility gaps, not claims that Google automatically rejects every
44 dp control.
[Apple accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility),
[Android accessibility](https://developer.android.com/guide/topics/ui/accessibility/views/apps-views).

### R7 — Native-glass interaction test is flaky

A fresh simulator build succeeded. The first `ARBITER_UI_SMOKE=1` run failed at
`mobile/src/main.cpp:416` with `Native editor Cancel failed`. A second full run
passed search, editor, repeated menu/tab dismissal, and private-dialog
concealment. Do not treat a passing retry as resolving the initial failure.
Determine whether it is animation/test timing or a missed real tap, then retain
a bounded condition-based regression check.

### Liquid Glass: good foundation, remaining validation

The implementation uses public `UIGlassEffect`/`UIVisualEffectView`, Regular
material, and no custom tint (`nativeglass_apple.mm:124–130`). Runtime iOS 26+
guards and non-Apple fallbacks exist. Reduce Transparency selects opaque Glacier,
and QML transitions consult Reduce Motion. The layered Icon Composer source is
present. These choices align with Apple's material hierarchy and legibility
guidance; adopting Liquid Glass itself is not an independent store-upload gate.
[Apple materials](https://developer.apple.com/design/human-interface-guidelines/materials).

Native simulator screenshot inspected:
`output/mobile-release-audit/ios-liquid-glass-tabs.png` (fabricated tab data).
It confirms native material and legible foregrounds in that configuration; its
plain background does not establish contrast over arbitrary web content.

Before design sign-off, test busy/bright pages, light/dark app and system modes,
system glass appearance preferences, Increase Contrast, Reduce Transparency,
Reduce Motion, text scaling, and screen readers. Keep glass on navigation and
functional overlays, avoid redundant nested glass backgrounds, and verify native
foreground colors remain consistent with the selected appearance. The current
bridge sets `interactive=YES` for every glass host; assess large-sheet touch
feedback rather than assuming pill-like feedback is suitable everywhere.
[Apple adoption guidance](https://developer.apple.com/documentation/technologyoverviews/adopting-liquid-glass).

## Verified strengths and limits

| Check | Result |
| --- | --- |
| iOS browser engine | WKWebView; no alternative-engine entitlement needed for this implementation |
| Android browser engine | System WebView; no GeckoView or bundled Chromium engine |
| iOS SDK | Existing device bundle uses iOS 27.2 SDK; exceeds the currently published iOS 26 SDK floor |
| Android target | Inspected APK targets API 37, above the current Play API 36 minimum |
| Android 16 KB compatibility | All 96 packaged native libraries pass the ELF alignment scan; APK zipalign check passes |
| Host mobile core | Existing test target passes |
| Native Swift crypto | PKCS#8, credential backup flags, signatures, AES-GCM vectors pass |
| Android instrumentation | 4 tests, 0 failures/errors/skips on disposable Android 15 AVD |
| iOS simulator interactions | Fresh build; first smoke run fails, full retry passes; see R7 |
| Glass API/platform guards | Present; iOS Regular material is untinted |
| Private data | iOS nonpersistent WebKit stores; Android separate restricted profiles, with documented WebView limitations |
| Transport | Android cancels TLS errors and rejects mixed content; sync implementations pin TLS and encrypt payloads |

The SDK floor is distinct from deployment minimum: supporting iOS 18 does not
conflict with compiling against a recent SDK. Browser HTTP exceptions are not
automatically a privacy violation; preserve ordinary HTTP browsing while keeping
credential sync and other application-controlled sensitive transfers protected.
[Apple SDK requirements](https://developer.apple.com/news/upcoming-requirements/),
[Play target requirements](https://support.google.com/googleplay/android-developer/answer/11926878),
[Android 16 KB guidance](https://developer.android.com/guide/practices/page-sizes).

The Android tests ran on API 35, not the targeted API 37 or a 16 KB runtime.
Alignment checks do not replace running on a 16 KB device. No physical-device
biometric/provider acceptance, IPv6-only network, iPad layout, foldable/tablet,
full accessibility matrix, or store-console validation was completed.

## Store-account and distribution decisions still required

- Resolve App Store distribution compatibility for Arbiter's GPL v3 license and
  the actual Qt modules/linkage. Qt specifically notes that some store terms may
  conflict with GPL/LGPL obligations. This needs a licensing/distribution decision,
  not an assumption that signing or commercial Qt alone resolves every license.
  [Qt obligations](https://www.qt.io/development/open-source-lgpl-obligations),
  [Qt licensing FAQ](https://www.qt.io/faq/qt-open-source-licensing).
- Complete encryption-export classification for the app and provider. CryptoKit,
  local AES-GCM credentials and encrypted sync are real encryption; do not set an
  exemption key without checking the applicable classification.
  [Apple export compliance](https://developer.apple.com/help/app-store-connect/manage-app-information/overview-of-export-compliance).
- Verify developer identity, agreements, browser/provider capabilities, app groups,
  signing certificates, Play App Signing, and any account-specific testing gate.
- Complete age/content ratings (including unrestricted browsing), privacy forms,
  support/contact URLs, screenshots, and accurate store descriptions. Android's
  credential provider currently advertises passkeys only; do not advertise
  system-wide Android password autofill as already implemented.
- Supply review instructions for LAN pairing and credential-provider activation;
  explain HTTP browsing exceptions and opportunistic background sync.
- Produce final signed archive/AAB artifacts, inspect merged permissions and SDK
  manifests, and run device tests for private locking, permission revocation,
  provider authentication, data deletion, rotation/insets, predictive Back and
  IPv6-only browsing. Revisit Qt-injected Bluetooth/storage/audio permissions;
  mark optional hardware features appropriately to avoid unnecessary filtering.

Recommended order: permission fixes and privacy manifests → policy/disclosures →
versioning and production pipelines → accessibility/glass regressions → device
matrix and store-console validation. Re-audit the actual signed artifacts before
submission.
