# UI translations

Arbiter ships English plus German, French, Italian, Spanish, Brazilian Portuguese,
Japanese, and Simplified Chinese. Translations are compiled into the executable;
there are no runtime translation requests or downloaded catalogs.

On startup, before onboarding or browser windows are created, Arbiter follows the
OS's ordered UI-language preferences. Regional variants match the supported
language (for example, Swiss Italian → Italian). Unsupported languages fall back
to the next supported preference, then English. Traditional Chinese is not mapped
to Simplified Chinese. A manual choice under Settings → Configuration → Interface
language overrides detection; restart to apply it to both QML and native menus.
The override is stored in the `General/language` TOML preference (`system` by default).
Language names are shown in their own language so the setting remains recognizable.

## Contributing

The initial non-English catalogs are machine-assisted drafts created using Google
Translate with the maintainer's permission, with corrections for common browser
labels. Native-speaker review is welcome, especially for security and privacy text.
The source English strings remain the reference. Do not translate product names,
URLs, command names, key combinations, or `%1`, `%2`, … placeholders. Keep the
meaning of warnings and irreversible actions intact. Qt's available standard
widget/QML/WebEngine catalogs are merged into each catalog at build time; missing
upstream Qt translations use English. Their licensing remains Qt's own.

1. Edit strings in C++ with `tr()` / `QObject::tr()` or in QML with `qsTr()`.
   Labels passed through a C++ helper need `QT_TRANSLATE_NOOP` with the actual
   translation context. Subclasses without `Q_OBJECT` must qualify the base
   class's `tr()` so extraction and runtime contexts agree.
2. Update messages: `cmake --build build-patched-app --target arbiter_lupdate`.
   All platform sources are scanned, including Objective-C++, regardless of
   the build platform. This is an explicit maintenance step, not a build side effect.
3. Edit `translations/arbiter_<locale>.ts` with Qt Linguist. Preserve message
   contexts and placeholders; mark reviewed translations finished. New unfinished
   messages fall back to English (`lrelease -nounfinished`).
4. Run `python3 tools/translations/validate.py`, then build normally. The validator
   checks coverage, duplicate messages, placeholders, URLs, and protected names.
5. Run the `localization` CTest, and check onboarding, settings, native menus, and
   popups in the language. Text length and font fallback vary by language.

To add a language, add its native name and locale code to
`translations/languages.json`, generate its TS catalog with `arbiter_lupdate`,
and translate it. The manifest controls the picker and build inputs. If the new
language needs right-to-left layout, audit and implement QML mirroring before
shipping it; the initial languages all use left-to-right layouts.

The automated `localization` test covers language negotiation, script-safe Chinese
fallback, persistence, restart behavior, bundled catalog loading, native/QML
contexts, and merged Qt standard strings. Test profiles use temporary directories.
