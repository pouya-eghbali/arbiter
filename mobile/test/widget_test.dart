import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter/services.dart';
import 'package:arbiter_mobile/browser_model.dart';
import 'package:arbiter_mobile/main.dart';

class MemoryStorage implements SessionStorage {
  String? value;
  @override
  Future<String?> read() async => value;
  @override
  Future<void> write(String data) async {
    value = data;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  setUp(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(
          const MethodChannel('com.arbiter.mobile/browser'),
          (_) async => null,
        );
  });
  testWidgets('start page exposes address, tabs, and compact actions', (
    tester,
  ) async {
    final model = BrowserModel(MemoryStorage());
    await tester.pumpWidget(ArbiterApp(model: model));
    await tester.pumpAndSettle();
    expect(find.text('Arbiter'), findsOneWidget);
    expect(find.text('Wikipedia'), findsOneWidget);
    expect(find.byTooltip('Show tabs'), findsOneWidget);
    await tester.tap(find.text('Search or enter address'));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('addressInput')), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
    model.dispose();
  });
}
