import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:arbiter_mobile/browser_model.dart';
import 'package:arbiter_mobile/native_browser.dart';
import 'package:arbiter_mobile/credential_vault.dart';
import 'package:arbiter_mobile/password_actions.dart';

class MemoryStorage implements SessionStorage {
  @override
  Future<String?> read() async => null;
  @override
  Future<void> write(String value) async {}
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  testWidgets(
    'password access needs authentication, respects its cooldown and supports every-time verification',
    (tester) async {
      final directory = Directory.systemTemp.createTempSync(
        'arbiter-password-ui-',
      );
      final model = BrowserModel(MemoryStorage())
        ..navigate('https://login.example');
      final engine = BrowserEngine();
      late BuildContext context;
      var granted = false, checks = 0, opens = 0;
      final fills = <Map>[];
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
            const MethodChannel('com.arbiter.mobile/browser'),
            (call) async {
              if (call.method == 'fill') {
                fills.add(call.arguments as Map);
                return true;
              }
              return null;
            },
          );
      final actions = PasswordActions(
        model,
        engine,
        () => context,
        () {},
        verifier: () async {
          checks++;
          return granted;
        },
        openVault: () async {
          opens++;
          final vault = CredentialVault.forTesting(
            File('${directory.path}/vault.enc'),
            Uint8List(32),
          );
          vault.state.update('fixture', 'fixture-device', {
            'kind': 'password',
            'origin': 'https://login.example',
            'username': 'fixture-user',
            'password': 'fixture-secret',
          });
          return vault;
        },
      );
      try {
        await tester.pumpWidget(
          MaterialApp(
            home: Builder(
              builder: (c) {
                context = c;
                return const Scaffold(body: SizedBox());
              },
            ),
          ),
        );
        actions.focus(model.activeTab.id, {'origin': 'https://evil.example'});
        expect(actions.visible, false);
        actions.focus(model.activeTab.id, {'origin': 'https://login.example'});
        await actions.fill();
        expect(checks, 1);
        expect(opens, 0);
        expect(fills, isEmpty);
        granted = true;
        Future<void> fill() async {
          actions.focus(model.activeTab.id, {
            'origin': 'https://login.example',
          });
          final result = actions.fill();
          await tester.pumpAndSettle();
          expect(find.text('fixture-user'), findsOneWidget);
          await tester.tap(find.text('fixture-user'));
          await tester.pumpAndSettle();
          await result;
        }

        await fill();
        expect(checks, 2);
        expect(opens, 1);
        expect(fills.last['origin'], 'https://login.example');
        await fill();
        expect(checks, 2);
        expect(opens, 1);
        model.passwordUnlockSeconds = 0;
        await fill();
        expect(checks, 3);
        await fill();
        expect(checks, 4);
        expect(fills.length, 4);
      } finally {
        actions.dispose();
        engine.dispose();
        model.dispose();
        await tester.pumpWidget(const SizedBox());
        directory.deleteSync(recursive: true);
      }
    },
  );
  testWidgets(
    'save waits for a successful form disappearance and cancels on background without opening credentials',
    (tester) async {
      final directory = Directory.systemTemp.createTempSync('arbiter-save-ui-');
      final model = BrowserModel(MemoryStorage())
        ..navigate('https://login.example');
      final engine = BrowserEngine();
      late BuildContext context;
      var hasField = true, opens = 0;
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
            const MethodChannel('com.arbiter.mobile/browser'),
            (call) async => call.method == 'hasPasswordField' ? hasField : null,
          );
      final actions = PasswordActions(
        model,
        engine,
        () => context,
        () {},
        verifier: () async => true,
        openVault: () async {
          opens++;
          return CredentialVault.forTesting(
            File('${directory.path}/vault.enc'),
            Uint8List(32),
          );
        },
      );
      try {
        await tester.pumpWidget(
          MaterialApp(
            home: Builder(
              builder: (c) {
                context = c;
                return const Scaffold(body: SizedBox());
              },
            ),
          ),
        );
        actions.submitted(model.activeTab.id, {
          'origin': 'https://login.example',
          'username': 'fixture',
          'password': 'fixture-secret',
        });
        await actions.finished(model.activeTab.id);
        await tester.pumpAndSettle();
        expect(find.text('Save this password?'), findsNothing);
        expect(opens, 0);
        hasField = false;
        final result = actions.finished(model.activeTab.id);
        await tester.pumpAndSettle();
        expect(find.text('Save this password?'), findsOneWidget);
        actions.background();
        await tester.pumpAndSettle();
        await result;
        expect(opens, 0);
        expect(File('${directory.path}/vault.enc').existsSync(), false);
        await tester.pump(const Duration(seconds: 1));
      } finally {
        actions.dispose();
        engine.dispose();
        model.dispose();
        await tester.pumpWidget(const SizedBox());
        directory.deleteSync(recursive: true);
      }
    },
  );
}
