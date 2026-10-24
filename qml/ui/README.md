# Shared UI controls

These components are exported by the application's `Arbiter` QML module. Use
`UiButton`, `UiTextField`, `UiCheckBox`, `UiComboBox`, `UiMenuItem`, `UiToolTip`
`UiColorPicker` and `UiImagePicker` in browser surfaces instead of styling Basic controls in
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
or white using relative luminance for contrast. Keep borders on neutral surfaces
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

Lucide sources, pinned revision, and regeneration instructions are in
`assets/icons/lucide/README.md`. The full ISC/Feather MIT notices ship in
`licenses/Lucide.txt`. Qt SVG is required for rendering on every platform.
