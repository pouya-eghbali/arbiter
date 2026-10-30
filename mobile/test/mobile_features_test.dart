import 'dart:convert';
import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:arbiter_mobile/browser_model.dart';
import 'package:arbiter_mobile/password_actions.dart';
import 'package:arbiter_mobile/main.dart';
import 'package:arbiter_mobile/l10n.dart';
import 'package:arbiter_mobile/native_browser.dart';

class MemoryStorage implements SessionStorage {
  String? data;
  @override
  Future<String?> read() async => data;
  @override
  Future<void> write(String value) async {
    data = value;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  setUp(
    () => TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(
          const MethodChannel('com.arbiter.mobile/browser'),
          (_) async => null,
        ),
  );
  tearDown(() => MobileStrings.language = 'system');
  test('generated passwords respect size and include every enabled character class', () {
    for (final length in [16, 20, 64]) {
      for (final symbols in [false, true]) {
        for (var i = 0; i < 20; i++) {
          final value = generatePassword(length: length, symbols: symbols);
          expect(value.length, length);
          expect(value, contains(RegExp('[a-z]')));
          expect(value, contains(RegExp('[A-Z]')));
          expect(value, contains(RegExp('[2-9]')));
          expect(value.contains(RegExp(r'[^a-zA-Z0-9]')), symbols);
        }
      }
    }
    expect(() => generatePassword(length: 8), throwsRangeError);
    expect(() => generatePassword(length: 65), throwsRangeError);
  });
  test(
    'sleep excludes the active tab, audio, edited forms and loading pages',
    () {
      final model = BrowserModel(MemoryStorage());
      final eligible = model.addTab(url: 'https://eligible.example');
      final audio = model.addTab(url: 'https://audio.example')
        ..playingAudio = true;
      final edited = model.addTab(url: 'https://edited.example')
        ..editedForm = true;
      final loading = model.addTab(url: 'https://loading.example')
        ..loading = true;
      final active = model.addTab(url: 'https://active.example');
      for (final tab in [eligible, audio, edited, loading, active]) {
        tab.lastUsed = DateTime(2000);
      }
      expect(model.sleepCandidates(DateTime.now()), [eligible]);
      eligible.sleeping = true;
      expect(model.sleepCandidates(DateTime.now()), isEmpty);
      eligible.sleeping = false;
      model.tabSleepSeconds = 0;
      expect(model.sleepCandidates(DateTime.now()), isEmpty);
      model.dispose();
    },
  );
  test('privacy, sleep, locale and onboarding choices survive restore without private data', () async {
    final storage = MemoryStorage(), model = BrowserModel(MemoryStorage());
    model.adblockEnabled = false;
    model.toggleSiteBlocking('https://example.com/path');
    model.language = 'fr';
    model.backgroundSync = false;
    model.tabSleepSeconds = 1800;
    model.onboardingDone = true;
    model.passwordUnlockSeconds = 60;
    model.savePasswords = false;
    model.suggestPasswords = false;
    model.addTab(url: 'https://private.example', private: true);
    await storage.write(jsonEncode(model.snapshot()));
    final restored = await BrowserModel.restore(storage);
    expect(restored.language, 'fr');
    expect(restored.backgroundSync, false);
    expect(restored.tabSleepSeconds, 1800);
    expect(restored.onboardingDone, true);
    expect(restored.adblockEnabled, false);
    expect(restored.adblockDisabledHosts, {'example.com'});
    expect(restored.passwordUnlockSeconds, 60);
    expect(restored.savePasswords, false);
    expect(restored.suggestPasswords, false);
    expect(storage.data, isNot(contains('private.example')));
    model.dispose();
    restored.dispose();
  });
  testWidgets(
    'onboarding follows selected locale and preserves language on completion',
    (tester) async {
      final model = BrowserModel(MemoryStorage())..language = 'fr';
      await tester.pumpWidget(ArbiterApp(model: model));
      await tester.pumpAndSettle();
      expect(find.text('Continuer'), findsOneWidget);
      expect(find.text('Ignorer la configuration'), findsOneWidget);
      await tester.tap(find.text('Ignorer la configuration'));
      await tester.pumpAndSettle();
      expect(model.onboardingDone, true);
      expect(find.byTooltip('Afficher les onglets'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      model.dispose();
    },
  );
  test(
    'placeholders are substituted after translating without translating values',
    () {
      MobileStrings.catalogs['de'] = {'Pair with {0}?': 'Mit {0} koppeln?'};
      expect(
        MobileStrings.text(
          'Pair with {0}?',
          locale: const Locale('de'),
          values: ['History'],
        ),
        'Mit History koppeln?',
      );
    },
  );
  test(
    'disposing onboarding cannot remove the new browser event receiver',
    () async {
      final old = BrowserEngine(), current = BrowserEngine();
      var events = 0;
      current.onEvent = (_, _) => events++;
      old.dispose();
      final reply = Completer<void>();
      ServicesBinding.instance.channelBuffers.push(
        'com.arbiter.mobile/browser',
        const StandardMethodCodec().encodeMethodCall(
          const MethodCall('event', {'id': 'fixture', 'type': 'activity'}),
        ),
        (_) => reply.complete(),
      );
      await reply.future;
      expect(events, 1);
      current.dispose();
    },
  );
}
