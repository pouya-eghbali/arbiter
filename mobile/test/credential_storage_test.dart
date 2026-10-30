import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:arbiter_mobile/credential_vault.dart';

void main() {
  test('local credentials are authenticated and serialized encrypted writes retain all edits', () async {
    final directory = await Directory.systemTemp.createTemp(
      'arbiter-encryption-test-',
    );
    final key = Uint8List.fromList(List.generate(32, (i) => i));
    final vault = CredentialVault.forTesting(
      File('${directory.path}/vault.enc'),
      key,
    );
    try {
      final a = vault.putPassword(
        'https://a.example',
        'one',
        'first-fixture-password',
      );
      final b = vault.putPassword(
        'https://b.example',
        'two',
        'second-fixture-password',
      );
      await Future.wait([a, b]);
      final bytes = await vault.file.readAsBytes();
      expect(
        utf8.decode(bytes, allowMalformed: true),
        isNot(contains('fixture-password')),
      );
      final clear = jsonDecode(
        utf8.decode(await openSync(bytes, key, 'ArbiterMobileVault/v1')),
      ) as Map<String, dynamic>;
      expect(
        CredentialState.parse(clear['state'] as Map<String, dynamic>)
            .visible
            .length,
        2,
      );
      await expectLater(
        openSync(bytes, Uint8List(32), 'ArbiterMobileVault/v1'),
        throwsA(anything),
      );
    } finally {
      vault.dispose();
      await directory.delete(recursive: true);
    }
  });
  test('locking during a queued save cannot overwrite the vault with a cleared key', () async {
    final directory = await Directory.systemTemp.createTemp(
      'arbiter-cancellation-test-',
    );
    final key = Uint8List.fromList(List.filled(32, 7));
    final vault = CredentialVault.forTesting(
      File('${directory.path}/vault.enc'),
      key,
    );
    try {
      await vault.putPassword(
        'https://example.com',
        'fixture',
        'keep-this-fixture',
      );
      final original = await vault.file.readAsBytes();
      final pending = vault.putPassword(
        'https://example.com',
        'fixture',
        'cancelled-edit',
      );
      vault.dispose();
      await expectLater(pending, throwsStateError);
      expect(await vault.file.readAsBytes(), original);
      final clear = utf8.decode(
        await openSync(original, key, 'ArbiterMobileVault/v1'),
      );
      expect(clear, contains('keep-this-fixture'));
      expect(clear, isNot(contains('cancelled-edit')));
      expect(vault.localKey.every((v) => v == 0), true);
    } finally {
      vault.dispose();
      await directory.delete(recursive: true);
    }
  });
  test(
    'independent vault sessions merge concurrent encrypted writes',
    () async {
      final directory = await Directory.systemTemp.createTemp(
        'arbiter-multi-session-',
      );
      final key = Uint8List.fromList(List.filled(32, 13));
      final file = File('${directory.path}/vault.enc');
      final first = CredentialVault.forTesting(file, key);
      final second = CredentialVault.forTesting(file, key);
      try {
        await Future.wait([
          first.putPassword('https://first.example', 'one', 'fixture-one'),
          second.putPassword('https://second.example', 'two', 'fixture-two'),
        ]);
        final document = jsonDecode(
          utf8.decode(
            await openSync(
              await file.readAsBytes(),
              key,
              'ArbiterMobileVault/v1',
            ),
          ),
        ) as Map<String, dynamic>;
        expect(
          CredentialState.parse(document['state'] as Map<String, dynamic>)
              .visible
              .length,
          2,
        );
      } finally {
        first.dispose();
        second.dispose();
        await directory.delete(recursive: true);
      }
    },
  );
}
