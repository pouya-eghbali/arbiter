import 'dart:convert';

import 'package:cryptography/cryptography.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:arbiter_mobile/credential_vault.dart';

Map<String, dynamic> password(String value) => {
  'kind': 'password',
  'origin': 'https://example.com',
  'username': 'me',
  'password': value,
};
CredentialState copy(CredentialState state) => CredentialState.parse(
  jsonDecode(jsonEncode(state.toJson())) as Map<String, dynamic>,
);
void main() {
  test('AES-256-GCM matches the NIST zero-key vector', () async {
    final sealed = await sealSync(
      List.filled(16, 0),
      List.filled(32, 0),
      '',
      nonce: List.filled(12, 0),
    );
    final hex = sealed.map((n) => n.toRadixString(16).padLeft(2, '0')).join();
    expect(
      hex,
      '000000000000000000000000cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919',
    );
    expect(await openSync(sealed, List.filled(32, 0), ''), List.filled(16, 0));
  });
  test(
    'tampering, wrong keys, and a different vault context fail authentication',
    () async {
      final key = List.filled(32, 7),
          bytes = utf8.encode('not a real credential');
      final sealed = await sealSync(bytes, key, 'ArbiterSync/v1/one');
      await expectLater(
        openSync(sealed, key, 'ArbiterSync/v1/two'),
        throwsA(isA<SecretBoxAuthenticationError>()),
      );
      await expectLater(
        openSync(sealed, List.filled(32, 8), 'ArbiterSync/v1/one'),
        throwsA(isA<SecretBoxAuthenticationError>()),
      );
      sealed[12] ^= 1;
      await expectLater(
        openSync(sealed, key, 'ArbiterSync/v1/one'),
        throwsA(isA<SecretBoxAuthenticationError>()),
      );
    },
  );
  test('concurrent edits converge without silently choosing a password', () {
    final a = CredentialState()
      ..update('login', 'desktop', password('initial'));
    final b = copy(a);
    a.update('login', 'desktop', password('from desktop'));
    b.update('login', 'phone', password('from phone'));
    final left = copy(a)..merge(b), right = copy(b)..merge(a);
    expect(canonical(left.toJson()), canonical(right.toJson()));
    expect(left.conflicts, ['login']);
    expect(left.visible, isEmpty);
    left.update('login', 'phone', password('resolved'));
    right.merge(left);
    left.merge(right);
    expect(left.conflicts, isEmpty);
    expect(canonical(left.toJson()), canonical(right.toJson()));
    expect(left.visible.single.value.data['password'], 'resolved');
  });
  test(
    'a concurrent deletion is retained, and stale replicas cannot resurrect it',
    () {
      final a = CredentialState()
        ..update('login', 'desktop', password('initial'));
      final stale = copy(a), b = copy(a);
      a.update('login', 'desktop', {}, deleted: true);
      b.update('login', 'phone', password('edited'));
      a.merge(b);
      expect(a.conflicts, ['login']);
      a.update('login', 'phone', {}, deleted: true);
      a.merge(stale);
      expect(a.visible, isEmpty);
      expect(a.conflicts, isEmpty);
      expect(a.records['login']!.single.deleted, true);
    },
  );
  test('equal clocks with different contents remain a conflict', () {
    final a = CredentialState()..update('login', 'desktop', password('a'));
    final b = CredentialState()..update('login', 'desktop', password('b'));
    a.merge(b);
    expect(a.conflicts, ['login']);
  });
  test('pairing accepts only local HTTPS and bounded credentials', () {
    for (final value in [
      'https://192.168.1.4:4321',
      'https://10.0.0.5:4321',
      'https://[fd00::1]:4321',
    ]) {
      expect(localSyncEndpoint(Uri.parse(value)), true);
    }
    for (final value in [
      'http://192.168.1.4:4321',
      'https://example.com',
      'https://192.168.1.4/x',
      'https://evil@192.168.1.4',
      'https://192.168.1.4?redirect=x',
    ]) {
      expect(localSyncEndpoint(Uri.parse(value)), false);
    }
    final data = {
      'v': 1,
      'endpoint': 'https://192.168.1.4:4321',
      'pin': 'a' * 64,
      'key': b64(List.filled(32, 1)),
      'code': b64(List.filled(32, 2)),
      'vaultId': '12345678-1234-1234-1234-123456789abc',
      'name': 'Desktop',
    };
    final pairing = PairingInfo.parse(
      'arbiter-pair://v1?data=${b64(utf8.encode(jsonEncode(data)))}',
    );
    expect(pairing.name, 'Desktop');
    expect(pairing.key.length, 32);
    data['endpoint'] = 'https://public.example';
    expect(
      () => PairingInfo.parse(
        'arbiter-pair://v1?data=${b64(utf8.encode(jsonEncode(data)))}',
      ),
      throwsFormatException,
    );
  });
}
