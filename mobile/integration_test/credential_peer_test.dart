import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:arbiter_mobile/credential_vault.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  const code = String.fromEnvironment('ARBITER_TEST_PAIRING_CODE');
  testWidgets(
    'phone pairs with a real native TLS peer and preserves offline edits',
    (tester) async {
      expect(code, isNotEmpty, reason: 'Run using tools/mobile/test_peer.py');
      var info = PairingInfo.parse(code);
      if (Platform.isAndroid) {
        info = PairingInfo(
          endpoint: info.endpoint.replace(host: '10.0.2.2'),
          pin: info.pin,
          key: info.key,
          vaultId: info.vaultId,
          code: info.code,
          name: info.name,
        );
      }
      final directory = await Directory.systemTemp.createTemp(
        'arbiter-vault-fixture-',
      );
      final key = Uint8List.fromList(List.generate(32, (i) => i));
      final vault = CredentialVault.forTesting(
        File('${directory.path}/vault.enc'),
        key,
      );
      try {
        await vault.putPassword(
          'https://phone.fixture.example',
          'phone-user',
          'fixture-phone-secret',
        );
        await vault.pair(info, 'Test phone');
        expect(vault.state.visible.length, 2);
        expect(
          vault.state.visible.any(
            (e) => e.value.data['origin'] == 'https://fixture.example',
          ),
          true,
        );
        final bytes = await vault.file.readAsBytes();
        expect(
          utf8.decode(bytes, allowMalformed: true),
          isNot(contains('fixture-phone-secret')),
        );
        final decoded = jsonDecode(
          utf8.decode(await openSync(bytes, key, 'ArbiterMobileVault/v1')),
        ) as Map<String, dynamic>;
        expect(
          CredentialState.parse(decoded['state'] as Map<String, dynamic>)
              .visible
              .length,
          2,
        );
        await vault.putPassword(
          'https://fixture.example',
          'fixture-user',
          'fixture-updated-on-phone',
        );
        await vault.sync();
        expect(
          vault.state.visible
              .where((e) => e.value.data['origin'] == 'https://fixture.example')
              .single
              .value
              .data['password'],
          'fixture-updated-on-phone',
        );
        final nativeId = vault.state.visible
            .where((e) => e.value.data['origin'] == 'https://fixture.example')
            .single
            .key;
        await vault.remove(nativeId);
        await vault.sync();
        expect(vault.state.visible.length, 1);
        // A wrong pin must fail before a credential payload is sent.
        final wrong = SyncPeerTransport(
          info.endpoint,
          '0' * 64,
          info.key,
          info.vaultId,
        );
        await expectLater(
          wrong.exchange(
            '/v1/sync',
            vault.peer!['token'] as String,
            vault.state.toJson(),
          ),
          throwsA(isA<HandshakeException>()),
        );
      } finally {
        vault.dispose();
        await directory.delete(recursive: true);
      }
    },
  );
}
