import 'l10n.dart';

import 'package:flutter/material.dart';

import 'browser_model.dart';
import 'browser_shell.dart' show ColorPickerDialog, Glyph;
import 'native_browser.dart';
import 'vault_screen.dart';

class Onboarding extends StatefulWidget {
  const Onboarding({super.key, required this.model});
  final BrowserModel model;
  @override
  State<Onboarding> createState() => _OnboardingState();
}

class _OnboardingState extends State<Onboarding> with WidgetsBindingObserver {
  int step = 0;
  final engine = BrowserEngine();
  BrowserModel get model => widget.model;
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void didChangeLocales(List<Locale>? locales) {
    model.changed();
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    engine.dispose();
    super.dispose();
  }

  Future<void> finish() async {
    await engine
        .command<void>(
          'configureBackgroundSync',
          data: {'enabled': model.backgroundSync},
        )
        .catchError((Object _) {});
    model.onboardingDone = true;
    model.changed();
    await model.flush();
  }

  @override
  Widget build(BuildContext c) => Scaffold(
    body: SafeArea(
      child: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(24, 14, 24, 0),
            child: Row(
              children: [
                Image.asset(
                  'assets/brand.png',
                  width: 30,
                  color: Theme.of(c).colorScheme.onSurface,
                ),
                const SizedBox(width: 10),
                const UiText('Arbiter'),
                const Spacer(),
                UiText(
                  "{0} / 5",
                  values: [(step + 1).toString()],
                  style: Theme.of(c).textTheme.labelLarge,
                ),
              ],
            ),
          ),
          Expanded(
            child: AnimatedSwitcher(
              duration: MediaQuery.disableAnimationsOf(c)
                  ? Duration.zero
                  : const Duration(milliseconds: 260),
              child: SingleChildScrollView(
                key: ValueKey(step),
                padding: const EdgeInsets.fromLTRB(26, 36, 26, 24),
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 520),
                  child: page(c),
                ),
              ),
            ),
          ),
          Padding(
            padding: const EdgeInsets.all(24),
            child: Row(
              children: [
                if (step > 0)
                  TextButton(
                    onPressed: () => setState(() => step--),
                    child: const UiText('Back'),
                  ),
                if (step == 0)
                  TextButton(
                    onPressed: finish,
                    child: const UiText('Skip setup'),
                  ),
                const Spacer(),
                FilledButton(
                  onPressed: () =>
                      step == 4 ? finish() : setState(() => step++),
                  child: UiText(step == 4 ? 'Start browsing' : 'Continue'),
                ),
              ],
            ),
          ),
        ],
      ),
    ),
  );
  Widget heading(String title, String body) => Column(
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      UiText(
        title,
        style: const TextStyle(
          fontSize: 33,
          fontWeight: FontWeight.w600,
          letterSpacing: -1,
        ),
      ),
      const SizedBox(height: 16),
      UiText(body, style: const TextStyle(fontSize: 17, height: 1.5)),
      const SizedBox(height: 30),
    ],
  );
  Widget page(BuildContext c) => switch (step) {
    0 => Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const SizedBox(height: 30),
        TweenAnimationBuilder<double>(
          tween: Tween(begin: 0, end: 1),
          duration: MediaQuery.disableAnimationsOf(c)
              ? Duration.zero
              : const Duration(milliseconds: 650),
          builder: (c, value, child) => Opacity(
            opacity: value,
            child: Transform.translate(
              offset: Offset(0, (1 - value) * 14),
              child: child,
            ),
          ),
          child: Image.asset(
            'assets/brand.png',
            width: 100,
            color: Theme.of(c).colorScheme.onSurface,
          ),
        ),
        const SizedBox(height: 28),
        heading(
          'A little less browser.\nA lot more web.',
          'Welcome to Arbiter. Your open-source browser, made for your phone. No Arbiter telemetry or data collection.',
        ),
        languagePicker(c),
      ],
    ),
    1 => Column(
      children: [
        heading(
          'Make yourself at home.',
          'A few defaults for a quieter, more private web. You can change them in Settings.',
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const UiText('Block ads and trackers'),
          value: model.adblockEnabled,
          onChanged: (v) {
            model.adblockEnabled = v;
            model.changed();
          },
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const UiText('Offer to save passwords'),
          value: model.savePasswords,
          onChanged: (v) {
            model.savePasswords = v;
            model.changed();
          },
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const UiText('Background sync on Wi-Fi'),
          subtitle: const UiText(
            'Only after you pair a desktop. No cloud account.',
          ),
          value: model.backgroundSync,
          onChanged: (v) {
            model.backgroundSync = v;
            model.changed();
          },
        ),
      ],
    ),
    2 => Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        heading(
          'Your colors. Your space.',
          'Follow your phone’s appearance or choose your own. Light and dark colors are saved separately.',
        ),
        SegmentedButton<ThemeMode>(
          segments: const [
            ButtonSegment(value: ThemeMode.system, label: UiText('System')),
            ButtonSegment(value: ThemeMode.light, label: UiText('Light')),
            ButtonSegment(value: ThemeMode.dark, label: UiText('Dark')),
          ],
          selected: {model.themeMode},
          onSelectionChanged: (s) {
            model.themeMode = s.first;
            model.changed();
          },
        ),
        const SizedBox(height: 24),
        for (final dark in [false, true])
          Card(
            elevation: 0,
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  Align(
                    alignment: Alignment.centerLeft,
                    child: UiText(
                      dark ? 'Dark appearance' : 'Light appearance',
                      style: Theme.of(c).textTheme.titleMedium,
                    ),
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(child: color(c, dark, false)),
                      const SizedBox(width: 12),
                      Expanded(child: color(c, dark, true)),
                    ],
                  ),
                ],
              ),
            ),
          ),
      ],
    ),
    3 => Column(
      children: [
        heading(
          'Bring your keys along.',
          'Pair with Arbiter on your desktop to sync passwords and eligible passkeys directly over the same Wi-Fi. Everything stays encrypted.',
        ),
        const Glyph('key-round', size: 56),
        const SizedBox(height: 24),
        FilledButton.tonalIcon(
          onPressed: () => Navigator.push(
            c,
            MaterialPageRoute<void>(
              builder: (_) => VaultScreen(model: model, engine: engine),
            ),
          ),
          icon: const Glyph('qr-code'),
          label: const UiText('Pair with desktop'),
        ),
        const SizedBox(height: 24),
        const Divider(),
        const SizedBox(height: 16),
        const UiText('Want Arbiter to open your links? This step is optional.'),
        const SizedBox(height: 16),
        OutlinedButton(
          onPressed: () => engine.command<void>('requestDefaultBrowser'),
          child: const UiText('Choose default browser'),
        ),
      ],
    ),
    _ => Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        heading(
          'You’re ready.',
          'The web gets the space. Everything else lives in the pill.',
        ),
        tutorial(
          'search',
          'Tap the address',
          'Search the web or enter a website address.',
        ),
        tutorial(
          'layers',
          'Swipe between tabs',
          'Swipe left or right on the pill to switch tabs. Swipe up to see them all.',
        ),
        tutorial(
          'shield-check',
          'You’re in control',
          'The shield pauses ad blocking for the current website.',
        ),
        tutorial(
          'eye-off',
          'Keep a little privacy',
          'Open private tabs from the tab overview. They lock when you leave the app.',
        ),
      ],
    ),
  };
  Widget tutorial(String icon, String title, String body) => Padding(
    padding: const EdgeInsets.only(bottom: 24),
    child: Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Glyph(icon, size: 26),
        const SizedBox(width: 18),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              UiText(
                title,
                style: const TextStyle(
                  fontWeight: FontWeight.w600,
                  fontSize: 17,
                ),
              ),
              const SizedBox(height: 5),
              UiText(body),
            ],
          ),
        ),
      ],
    ),
  );
  Widget color(BuildContext c, bool dark, bool background) {
    final value = dark
        ? (background ? model.darkBackground : model.darkAccent)
        : (background ? model.lightBackground : model.lightAccent);
    return OutlinedButton.icon(
      icon: Container(
        width: 18,
        height: 18,
        decoration: BoxDecoration(
          color: Color(value),
          shape: BoxShape.circle,
          border: Border.all(color: Theme.of(c).colorScheme.outline),
        ),
      ),
      label: UiText(background ? 'Background' : 'Accent'),
      onPressed: () async {
        final picked = await showDialog<Color>(
          context: c,
          builder: (_) => ColorPickerDialog(
            initial: Color(value),
            title: background ? 'Background color' : 'Accent color',
          ),
        );
        if (picked == null) return;
        if (dark) {
          if (background) {
            model.darkBackground = picked.toARGB32();
          } else {
            model.darkAccent = picked.toARGB32();
          }
        } else {
          if (background) {
            model.lightBackground = picked.toARGB32();
          } else {
            model.lightAccent = picked.toARGB32();
          }
        }
        model.changed();
      },
    );
  }

  Widget languagePicker(BuildContext c) => DropdownButtonFormField<String>(
    initialValue: model.language,
    decoration: InputDecoration(labelText: tr('Language')),
    items: [
      for (final entry in languageNames.entries)
        DropdownMenuItem(
          value: entry.key,
          child: entry.key == "system"
              ? const UiText("System language")
              : Text(entry.value),
        ),
    ],
    onChanged: (v) {
      if (v != null) {
        model.language = v;
        model.changed();
        engine.command<void>('configureLanguage', data: {'language': v});
      }
    },
  );
}

const languageNames = {
  'system': 'System language',
  'en': 'English',
  'de': 'Deutsch',
  'fr': 'Français',
  'it': 'Italiano',
  'es': 'Español',
  'pt': 'Português',
  'ja': '日本語',
  'zh': '中文',
};
