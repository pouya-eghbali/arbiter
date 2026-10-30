import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:arbiter_mobile/main.dart';
import 'package:arbiter_mobile/browser_model.dart';
import 'package:arbiter_mobile/native_browser.dart';

class MemoryStorage implements SessionStorage {
  @override
  Future<String?> read() async => null;
  @override
  Future<void> write(String value) async {}
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  testWidgets(
    'headless iOS engine initializes and shares the native vault transaction gate',
    (tester) async {
      final model = BrowserModel(MemoryStorage())..onboardingDone = true;
      await tester.pumpWidget(ArbiterApp(model: model));
      await tester.pumpAndSettle();
      final engine = BrowserEngine();
      if (Platform.isIOS) {
        expect(await engine.command<bool>('backgroundSyncSmoke'), true);
      }
      await tester.pumpWidget(const SizedBox());
      engine.dispose();
      model.dispose();
    },
  );
}
