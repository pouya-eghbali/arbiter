import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:arbiter_mobile/browser_model.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  testWidgets('app and credential-provider Keychain group is usable', (
    tester,
  ) async {
    if (!Platform.isIOS) return;
    final configuration = await const MethodChannel(
      'com.arbiter.mobile/browser',
    ).invokeMapMethod<String, dynamic>('vaultConfiguration');
    expect(configuration?['directory'], isNotEmpty);
    expect(configuration?['group'], isNotEmpty);
    final storage = FlutterSecureStorage(
      iOptions: IOSOptions(
        groupId: configuration!['group'] as String,
        accountName: 'flutter_secure_storage_service',
        accessibility: KeychainAccessibility.unlocked_this_device,
      ),
    );
    final key = 'arbiter-test-${newId()}';
    try {
      await storage.write(key: key, value: 'fixture-keychain-value');
      expect(await storage.read(key: key), 'fixture-keychain-value');
    } finally {
      await storage.delete(key: key);
    }
  });
}
