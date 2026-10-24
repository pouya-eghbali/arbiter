# Lucide UI icons

Source: https://github.com/lucide-icons/lucide
Pinned revision: `7faf87628a1ad6175975bc595344337ce14a7db4` (also recorded in `REVISION`).

These SVGs are unmodified upstream files. The complete ISC license and inherited
Feather MIT notice are preserved in `licenses/Lucide.txt`, which is bundled in the
application at `:/licenses/Lucide.txt`.

`python3 tools/icons/generate.py` regenerates `qml/ui/LucideIcons.js` from these
files without network access. Check in both SVG sources and the generated registry.
Only this small subset is bundled. To add an icon, retrieve its SVG from the pinned
revision, then regenerate. Updating upstream requires updating `REVISION` and the
full license notices as well.

`UiIcon` produces a tinted SVG data URL from the registry. Qt's SVG image plugin
renders it at the requested size with caching, including under the software scene
graph. No runtime fetches, icon fonts, private Qt controls or GPU effects are used.
