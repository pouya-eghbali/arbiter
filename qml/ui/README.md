# Shared UI controls

These components are exported by the application's `Arbiter` QML module. Use
`UiButton`, `UiIconButton`, `UiTextField`, `UiCheckBox`, `UiComboBox`, `UiMenuItem`, `UiToolTip`
`UiMenu`, `UiPopup`, `UiColorPicker` and `UiImagePicker` in browser surfaces instead of styling Basic controls in
individual screens. `Theme` is the shared color source and `GlassSurface`
provides popup material.

Buttons support `primary`, `flat`, `highlighted`, checkable states and normal
Qt keyboard/accessibility behavior. Text fields preserve standard password and
read-only behavior. Displayed user/site text uses plain text, not rich text.
A consumer may override presentation for a special purpose (for example an
invisible navigation reveal target), while keeping standard interaction behavior.

Light and dark accent and background colors are stored independently by `Appearance`.
`UiColorPicker` uses Qt Quick Dialogs for the centered in-app color picker. It only
emits a new color when accepted; cancellation keeps the saved color. Include the
`QtQuick.Dialogs` QML module when deploying the app. Background-derived colors
are centralized in `Theme`, including dense and glass surfaces. Selection
and focus colors follow the active accent. Text on a solid accent chooses black
or white using relative luminance for contrast. Disabled primary buttons mute only
their fill and choose text/icon colors against that fill; whole-button opacity
must not reduce their foreground contrast. Keep borders on neutral surfaces
subtle; reserve accent borders for keyboard focus.

`UiImagePicker` provides a local-file chooser, preview, and removal control. It emits
file selection for `Appearance` to validate and copy; cancellation preserves the
saved image. `BrowserBackground` paints the active image with a centered crop,
using the theme background color while loading or if the image is unavailable.

`UiIcon` renders the bundled Lucide SVG subset with a theme-aware `color` and a
24-pixel default size. Use canonical Lucide names; `user`, `phone`, and `key` remain
aliases for existing credential cards. `UiButton` supports `iconName`, `iconSize`
(default 20), and `iconColor`, with or without a text label. Icon-only buttons must
set a descriptive `Accessible.name`; decorative icons are ignored by screen readers.
Use the shared control instead of text glyphs or bespoke drawings for UI actions.
`UiIconButton` is the compact 32-pixel action control. Supply `iconName` and the
required `label`, which provides both its tooltip and accessible name. Keep text
buttons for actions whose meaning needs a visible label, such as confirmations.

Lucide sources, pinned revision, and regeneration instructions are in
`assets/icons/lucide/README.md`. The full ISC/Feather MIT notices ship in
`licenses/Lucide.txt`. Qt SVG is required for rendering on every platform.

Page overlays use `GlassSurface` for their backgrounds: a live, rounded backdrop
blur, background-color tint, broad sheen and soft ambient/contact shadows. Surface
blur uses a shared 64-pixel radius and captures surrounding pixels before cropping
to the rounded shape, including the collapsed pill. The mask uses an explicit
`ShaderEffectSource`; hiding a layered mask rectangle would hide the blur output.
Capture coordinates track ancestor geometry so moving/animating overlays stay aligned.
No animation timer is needed for a stationary surface. Surface perimeters have no
border; accent outlines are reserved for keyboard focus inside
controls. `Theme.panelRadius`, `menuRadius`, and `tooltipRadius` keep dialog, menu,
and tooltip shapes consistent. `dense` increases tint for compact text-heavy menus
without replacing their backdrop with an opaque plate.

`Main.glassBackdrop` is a clean composite of that window's page area and split
panes (or Settings while it is open), excluding floating overlays. Start-page prompts sample only the background
image to avoid capturing themselves. A surface samples only its own window; private
locking clears its backdrop. Sampling stops while the surface is hidden. Use
`sourceItem: null` for surfaces that must never sample page contents (the private
unlock panel, for example). Without a page or GPU effects, surfaces use the same
background-color tint as an opaque fallback. Custom menus/popovers use `Popup.Item`
so native popup-window frames cannot introduce mismatched edges or corners.

`UiMenu` provides the shared glass menu, a readable minimum width and a scrollable
height constrained to the owning window. Use it for application and page menus.
`UiPopup` and `UiMenu` share short opacity transitions from `Theme`. The pill
animates its presence separately from its expansion, so hiding it does not cut
off the collapse animation. Hidden/closing pill controls stop accepting input.
`Theme.reducedMotion` follows Qt's live OS accessibility motion preference; shared
transition durations become zero when reduced motion is enabled. Private locking
always hides sensitive UI immediately, independent of animation settings.

`glasssurface_test` checks actual rendered pixels: stripe suppression on large and
small surfaces, live backdrop changes, ancestor movement and cross-window isolation.
Run it with a hardware Qt Quick backend for pixel validation (on macOS,
`QT_QPA_PLATFORM=cocoa QSG_RHI_BACKEND=metal build-patched-tests/glasssurface/glasssurface_test`).
The default software CTest run checks the fallback and skips GPU pixel assertions.
