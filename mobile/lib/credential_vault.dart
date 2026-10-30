import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:crypto/crypto.dart' as hashes;
import 'package:cryptography/cryptography.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:path_provider/path_provider.dart';
import 'package:flutter/services.dart';

import 'browser_model.dart';

const maxSyncBytes = 16 * 1024 * 1024;
final _cipher = AesGcm.with256bits();
String b64(List<int> bytes) => base64Url.encode(bytes).replaceAll('=', '');
Uint8List unb64(String value) => base64Url.decode(base64Url.normalize(value));

Future<Uint8List> sealSync(
  List<int> clear,
  List<int> key,
  String context, {
  List<int>? nonce,
}) async {
  if (key.length != 32 || clear.length > maxSyncBytes) {
    throw const FormatException('Invalid encryption input');
  }
  final box = await _cipher.encrypt(
    clear,
    secretKey: SecretKey(key),
    nonce: nonce,
    aad: utf8.encode(context),
  );
  return Uint8List.fromList([
    ...box.nonce,
    ...box.cipherText,
    ...box.mac.bytes,
  ]);
}

Future<Uint8List> openSync(
  List<int> sealed,
  List<int> key,
  String context,
) async {
  if (key.length != 32 ||
      sealed.length < 28 ||
      sealed.length > maxSyncBytes + 28) {
    throw const FormatException('Invalid encrypted message');
  }
  final box = SecretBox(
    sealed.sublist(12, sealed.length - 16),
    nonce: sealed.sublist(0, 12),
    mac: Mac(sealed.sublist(sealed.length - 16)),
  );
  return Uint8List.fromList(
    await _cipher.decrypt(
      box,
      secretKey: SecretKey(key),
      aad: utf8.encode(context),
    ),
  );
}

class CredentialVersion {
  CredentialVersion({
    required this.clock,
    required this.data,
    this.deleted = false,
  });
  final Map<String, int> clock;
  final Map<String, dynamic> data;
  final bool deleted;
  Map<String, Object> toJson() => {
    'clock': clock,
    'data': data,
    'deleted': deleted,
  };
  static CredentialVersion parse(Map<String, dynamic> json) {
    final raw = json['clock'] as Map;
    if (raw.isEmpty || raw.length > 32) {
      throw const FormatException('Invalid version vector');
    }
    final clock = <String, int>{};
    for (final entry in raw.entries) {
      if (entry.key is! String ||
          (entry.key as String).isEmpty ||
          (entry.key as String).length > 80 ||
          entry.value is! int ||
          entry.value <= 0 ||
          entry.value > 9007199254740991) {
        throw const FormatException('Invalid version vector');
      }
      clock[entry.key as String] = entry.value as int;
    }
    final data = Map<String, dynamic>.from(json['data'] as Map);
    if (jsonEncode(data).length > 131072) {
      throw const FormatException('Credential is too large');
    }
    if (json['deleted'] is! bool) {
      throw const FormatException('Invalid credential deletion flag');
    }
    final deleted = json['deleted'] == true;
    if (!deleted) {
      if (data['kind'] == 'password') {
        if (!isWebAddress(data['origin'] as String? ?? '') ||
            data['username'] is! String ||
            data['password'] is! String ||
            (data['username'] as String).length > 4096 ||
            (data['password'] as String).length > 65536) {
          throw const FormatException('Invalid password record');
        }
      } else if (data['kind'] == 'passkey') {
        if (data['rpId'] is! String ||
            (data['rpId'] as String).length > 253 ||
            data['credentialId'] is! String ||
            data['privateKey'] is! String ||
            data['userId'] is! String) {
          throw const FormatException('Invalid passkey record');
        }
        if (unb64(data['credentialId'] as String).length != 32 ||
            unb64(data['privateKey'] as String).isEmpty ||
            unb64(data['privateKey'] as String).length > 16384 ||
            unb64(data['userId'] as String).isEmpty ||
            unb64(data['userId'] as String).length > 64) {
          throw const FormatException('Invalid passkey key material');
        }
      } else {
        throw const FormatException('Unknown credential type');
      }
    }
    return CredentialVersion(clock: clock, data: data, deleted: deleted);
  }
}

bool dominates(Map<String, int> a, Map<String, int> b) =>
    b.entries.every((e) => (a[e.key] ?? 0) >= e.value) &&
    a.entries.any((e) => e.value > (b[e.key] ?? 0));
String canonical(Object? value) {
  if (value is Map) {
    final keys = value.keys.cast<String>().toList()..sort();
    return '{${keys.map((k) => '${jsonEncode(k)}:${canonical(value[k])}').join(',')}}';
  }
  if (value is List) return '[${value.map(canonical).join(',')}]';
  return jsonEncode(value);
}

/// A multi-value register per credential. Concurrent edits and delete/edit
/// conflicts are retained until a user explicitly resolves them.
class CredentialState {
  CredentialState({Map<String, List<CredentialVersion>>? records})
    : records = records ?? {};
  final Map<String, List<CredentialVersion>> records;
  Map<String, Object> toJson() => {
    'v': 1,
    'records': records.map(
      (k, v) => MapEntry(k, v.map((r) => r.toJson()).toList()),
    ),
  };
  static CredentialState parse(Map<String, dynamic> json) {
    if (json['v'] != 1) {
      throw const FormatException('Unsupported credential sync version');
    }
    final raw = json['records'] as Map;
    if (raw.length > 10000) {
      throw const FormatException('Too many credential records');
    }
    final records = <String, List<CredentialVersion>>{};
    for (final entry in raw.entries) {
      if (entry.key is! String ||
          (entry.key as String).length > 1024 ||
          entry.value is! List ||
          (entry.value as List).length > 32) {
        throw const FormatException('Invalid credential register');
      }
      records[entry.key as String] = (entry.value as List)
          .map(
            (v) => CredentialVersion.parse(Map<String, dynamic>.from(v as Map)),
          )
          .toList();
    }
    return CredentialState(records: records);
  }

  void merge(CredentialState other) {
    for (final entry in other.records.entries) {
      final unique = <String, CredentialVersion>{};
      for (final version in [...?records[entry.key], ...entry.value]) {
        unique[canonical(version.toJson())] = version;
      }
      final all = unique.values.toList();
      final retained =
          all
              .where(
                (v) => !all.any((other) => dominates(other.clock, v.clock)),
              )
              .toList()
            ..sort(
              (a, b) => canonical(a.toJson()).compareTo(canonical(b.toJson())),
            );
      if (retained.length > 32) {
        throw const FormatException('Too many conflicting changes');
      }
      records[entry.key] = retained;
    }
  }

  void update(
    String id,
    String device,
    Map<String, dynamic> data, {
    bool deleted = false,
  }) {
    final clock = <String, int>{};
    for (final version in records[id] ?? <CredentialVersion>[]) {
      for (final e in version.clock.entries) {
        if (e.value > (clock[e.key] ?? 0)) clock[e.key] = e.value;
      }
    }
    clock[device] = (clock[device] ?? 0) + 1;
    records[id] = [
      CredentialVersion(clock: clock, data: data, deleted: deleted),
    ];
  }

  List<MapEntry<String, CredentialVersion>> get visible => [
    for (final e in records.entries)
      if (e.value.length == 1 && !e.value.single.deleted)
        MapEntry(e.key, e.value.single),
  ];
  List<String> get conflicts => [
    for (final e in records.entries)
      if (e.value.length > 1) e.key,
  ];
}

class PairingInfo {
  PairingInfo({
    required this.endpoint,
    required this.pin,
    required this.key,
    required this.vaultId,
    required this.code,
    required this.name,
  });
  final Uri endpoint;
  final String pin, vaultId, code, name;
  final Uint8List key;
  static PairingInfo parse(String value) {
    if (value.length > 16000) {
      throw const FormatException('Invalid pairing code');
    }
    final uri = Uri.parse(value.trim());
    if (uri.scheme != 'arbiter-pair' || uri.host != 'v1') {
      throw const FormatException('Not an Arbiter pairing code');
    }
    final data = jsonDecode(
      utf8.decode(unb64(uri.queryParameters['data'] ?? '')),
    ) as Map<String, dynamic>;
    final endpoint = Uri.parse(data['endpoint'] as String);
    if (data['v'] != 1 || !localSyncEndpoint(endpoint)) {
      throw const FormatException('Pairing requires a local HTTPS endpoint');
    }
    final pin = data['pin'] as String;
    final key = unb64(data['key'] as String);
    final code = data['code'] as String;
    final vaultId = data['vaultId'] as String;
    if (!RegExp(r'^[0-9a-f]{64}$').hasMatch(pin) ||
        key.length != 32 ||
        unb64(code).length != 32 ||
        !RegExp(r'^[a-zA-Z0-9-]{16,80}$').hasMatch(vaultId)) {
      throw const FormatException('Invalid pairing credentials');
    }
    return PairingInfo(
      endpoint: endpoint,
      pin: pin,
      key: key,
      vaultId: vaultId,
      code: code,
      name: data['name'] as String? ?? 'Arbiter desktop',
    );
  }
}

bool localSyncEndpoint(Uri uri) {
  if (uri.scheme != 'https' ||
      uri.userInfo.isNotEmpty ||
      uri.hasQuery ||
      uri.hasFragment ||
      uri.path.isNotEmpty && uri.path != '/') {
    return false;
  }
  final host = uri.host.toLowerCase();
  if (host == 'localhost' ||
      host.endsWith('.local') ||
      host == '::1' ||
      host.startsWith('fe80:') ||
      host.startsWith('fc') && host.contains(':') ||
      host.startsWith('fd') && host.contains(':')) {
    return true;
  }
  final parts = host.split('.').map(int.tryParse).toList();
  if (parts.length != 4 || parts.any((p) => p == null || p < 0 || p > 255)) {
    return false;
  }
  return parts[0] == 10 ||
      parts[0] == 127 ||
      parts[0] == 192 && parts[1] == 168 ||
      parts[0] == 172 && parts[1]! >= 16 && parts[1]! <= 31;
}

class CredentialVault {
  CredentialVault._(
    this.file,
    this.localKey,
    this.deviceId,
    this.state,
    this.peer, {
    this.nativeIdentities = false,
  }) : _peerBase = canonical(peer);

  /// Isolated test fixture; production callers use [open] and the OS vault.
  factory CredentialVault.forTesting(File file, Uint8List key) =>
      CredentialVault._(
        file,
        Uint8List.fromList(key),
        newId(),
        CredentialState(),
        null,
      );
  final bool nativeIdentities;
  final File file;
  final Uint8List localKey;
  final String deviceId;
  CredentialState state;
  Map<String, dynamic>? peer;
  bool _disposed = false;
  static final Map<String, Future<void>> _fileWrites = {};
  static int _peerEpoch = 0;
  String _peerBase;
  SyncPeerTransport? _transport;
  void _ensureOpen() {
    if (_disposed) throw StateError('Vault is locked');
  }

  static const _native = MethodChannel('com.arbiter.mobile/browser');
  static Future<CredentialVault> open() async {
    final configuration = Platform.isIOS
        ? await _native.invokeMapMethod<String, dynamic>('vaultConfiguration')
        : null;
    final directory = Platform.isIOS
        ? Directory(configuration!['directory'] as String)
        : await getApplicationSupportDirectory();
    final keys = FlutterSecureStorage(
      aOptions: const AndroidOptions(resetOnError: false),
      iOptions: IOSOptions(
        accessibility: KeychainAccessibility.unlocked_this_device,
        groupId: configuration?['group'] as String?,
        accountName: 'flutter_secure_storage_service',
      ),
    );
    await directory.create(recursive: true);
    final file = File('${directory.path}/credentials-v1.enc');
    final lock = await File('${file.path}.lock').open(mode: FileMode.append);
    var locked = false;
    String? processToken;
    try {
      if (Platform.isIOS) {
        processToken = await _native.invokeMethod<String>('acquireVaultLock');
      }
      await lock.lock(FileLock.exclusive);
      locked = true;
      var savedKey = await keys.read(key: 'arbiter-vault-key-v1');
      if (savedKey == null) {
        if (await file.exists()) {
          throw const FormatException(
            'The device vault key is unavailable. Restore this device before continuing.',
          );
        }
        savedKey = b64(
          await _cipher.newSecretKey().then((k) => k.extractBytes()),
        );
        await keys.write(key: 'arbiter-vault-key-v1', value: savedKey);
      }
      final key = unb64(savedKey);
      if (key.length != 32) {
        throw const FormatException('Invalid device vault key');
      }
      Map<String, dynamic> saved = {};
      if (await file.exists()) {
        if (await file.length() > maxSyncBytes + 28) {
          throw const FormatException('Device vault is too large');
        }
        saved = jsonDecode(
          utf8.decode(
            await openSync(
              await file.readAsBytes(),
              key,
              'ArbiterMobileVault/v1',
            ),
          ),
        ) as Map<String, dynamic>;
      }
      return CredentialVault._(
        file,
        key,
        saved['device'] as String? ?? newId(),
        saved['state'] == null
            ? CredentialState()
            : CredentialState.parse(
                Map<String, dynamic>.from(saved['state'] as Map),
              ),
        saved['peer'] == null
            ? null
            : Map<String, dynamic>.from(saved['peer'] as Map),
        nativeIdentities: true,
      );
    } finally {
      try {
        if (locked) await lock.unlock();
      } finally {
        await lock.close();
        if (processToken != null) {
          await _native.invokeMethod<void>('releaseVaultLock', {
            'token': processToken,
          });
        }
      }
    }
  }

  static Future<CredentialVault?> openExistingForSync() async {
    final configuration = Platform.isIOS
        ? await _native.invokeMapMethod<String, dynamic>('vaultConfiguration')
        : null;
    final directory = Platform.isIOS
        ? Directory(configuration!['directory'] as String)
        : await getApplicationSupportDirectory();
    if (!await File('${directory.path}/credentials-v1.enc').exists()) {
      return null;
    }
    return open();
  }

  Future<void> disconnect() async {
    _ensureOpen();
    ++_peerEpoch;
    _transport?.close();
    peer = null;
    await save();
  }

  Future<void> save() {
    _ensureOpen();
    final basePeer = _peerBase;
    final snapshotPeer = canonical(peer);
    final snapshot = jsonEncode({
      'device': deviceId,
      'state': state.toJson(),
      'peer': peer,
    });
    final operation = (_fileWrites[file.path] ?? Future<void>.value()).then((
      _,
    ) async {
      _ensureOpen();
      final key = Uint8List.fromList(localKey);
      final temporary = File('${file.path}.${newId()}.tmp');
      final lock = await File('${file.path}.lock').open(mode: FileMode.append);
      Uint8List? plain;
      var locked = false;
      String? processToken;
      try {
        if (Platform.isIOS && nativeIdentities) {
          processToken = await _native.invokeMethod<String>('acquireVaultLock');
        }
        await lock.lock(FileLock.exclusive);
        locked = true;
        _ensureOpen();
        final document = jsonDecode(snapshot) as Map<String, dynamic>;
        // The OS credential provider can edit the vault in another process.
        // Merge under the same advisory lock before the atomic replacement.
        if (await file.exists()) {
          final latest = jsonDecode(
            utf8.decode(
              await openSync(
                await file.readAsBytes(),
                key,
                'ArbiterMobileVault/v1',
              ),
            ),
          ) as Map<String, dynamic>;
          if (canonical(latest['peer']) != basePeer &&
              snapshotPeer == basePeer) {
            document['peer'] = latest['peer'];
            if (canonical(peer) == snapshotPeer) {
              peer = latest['peer'] == null
                  ? null
                  : Map<String, dynamic>.from(latest['peer'] as Map);
            }
          }
          final merged = CredentialState.parse(
            latest['state'] as Map<String, dynamic>,
          );
          merged.merge(
            CredentialState.parse(document['state'] as Map<String, dynamic>),
          );
          document['state'] = merged.toJson();
          _ensureOpen();
          state.merge(merged);
        }
        plain = Uint8List.fromList(utf8.encode(jsonEncode(document)));
        final sealed = await sealSync(plain, key, 'ArbiterMobileVault/v1');
        _ensureOpen();
        await temporary.writeAsBytes(sealed, flush: true);
        _ensureOpen();
        await temporary.rename(file.path);
        _peerBase = canonical(document['peer']);
      } finally {
        key.fillRange(0, key.length, 0);
        plain?.fillRange(0, plain.length, 0);
        try {
          if (locked) await lock.unlock();
        } finally {
          await lock.close();
          if (processToken != null) {
            await _native.invokeMethod<void>('releaseVaultLock', {
              'token': processToken,
            });
          }
        }
        if (await temporary.exists()) await temporary.delete();
      }
      if (nativeIdentities && !_disposed) {
        if (Platform.isIOS) {
          await _native
              .invokeMethod<void>('credentialsChanged')
              .catchError((Object _) {});
        }
        if (Platform.isAndroid) {
          final entries = [
            for (final entry in state.visible)
              {
                'id': entry.key,
                for (final field in [
                  'kind',
                  'origin',
                  'rpId',
                  'username',
                  'credentialId',
                  'userId',
                ])
                  if (entry.value.data.containsKey(field))
                    field: entry.value.data[field],
              },
          ];
          await _native
              .invokeMethod<void>('credentialIndex', {'entries': entries})
              .catchError((Object _) {});
        }
      }
    });
    final settled = operation.then<void>(
      (_) {},
      onError: (Object _, StackTrace _) {},
    );
    _fileWrites[file.path] = settled;
    unawaited(
      settled.whenComplete(() {
        if (identical(_fileWrites[file.path], settled)) {
          _fileWrites.remove(file.path);
        }
      }),
    );
    return operation;
  }

  Future<void> putPassword(
    String origin,
    String username,
    String password,
  ) async {
    _ensureOpen();
    final uri = Uri.parse(origin);
    if (uri.scheme != 'https' || !isWebAddress(origin) || password.isEmpty) {
      throw const FormatException('Enter a website and password');
    }
    final site = uri.origin;
    int stringSize(String value) {
      final n = utf8.encode(value).length;
      return n +
          (n < 24
              ? 1
              : n < 256
              ? 2
              : n < 65536
              ? 3
              : 5);
    }

    if (username.length > 512 ||
        35 + stringSize(site) + stringSize(username) + stringSize(password) >
            2560) {
      throw const FormatException(
        'This login exceeds the cross-platform OS vault size limit (2560 encoded bytes).',
      );
    }
    final id =
        'password:${hashes.sha256.convert(utf8.encode('$site\u0000$username'))}';
    state.update(id, deviceId, {
      'kind': 'password',
      'origin': site,
      'username': username,
      'password': password,
    });
    await save();
  }

  Future<void> remove(String id) async {
    _ensureOpen();
    state.update(id, deviceId, {}, deleted: true);
    await save();
  }

  Future<void> resolve(String id, CredentialVersion version) async {
    _ensureOpen();
    state.update(id, deviceId, version.data, deleted: version.deleted);
    await save();
  }

  Future<void> pair(PairingInfo info, String deviceName) async {
    _ensureOpen();
    final transport = _transport = SyncPeerTransport(
      info.endpoint,
      info.pin,
      info.key,
      info.vaultId,
    );
    final result = await transport.exchange('/v1/pair', info.code, {
      'deviceId': deviceId,
      'name': deviceName,
    });
    _ensureOpen();
    final token = result['token'] as String;
    if (unb64(token).length != 32) {
      throw const FormatException('Invalid device authorization');
    }
    ++_peerEpoch;
    peer = {
      'endpoint': info.endpoint.toString(),
      'pin': info.pin,
      'key': b64(info.key),
      'vaultId': info.vaultId,
      'token': token,
      'name': info.name,
    };
    await save();
    await sync();
  }

  Future<void> sync() async {
    _ensureOpen();
    final epoch = _peerEpoch;
    final p = peer;
    if (p == null) throw const FormatException('Pair with your desktop first');
    final transport = _transport = SyncPeerTransport(
      Uri.parse(p['endpoint'] as String),
      p['pin'] as String,
      unb64(p['key'] as String),
      p['vaultId'] as String,
    );
    final response = await transport.exchange(
      '/v1/sync',
      p['token'] as String,
      state.toJson(),
    );
    _ensureOpen();
    if (epoch != _peerEpoch) throw StateError('Device pairing changed');
    final merged = CredentialState.parse(response);
    // Merge into a new state before saving; malformed input never partially
    // mutates the device vault or discards a local conflicting edit.
    final next = CredentialState.parse(state.toJson());
    next.merge(merged);
    state = next;
    await save();
  }

  void dispose() {
    _disposed = true;
    _transport?.close();
    localKey.fillRange(0, localKey.length, 0);
    state.records.clear();
    peer = null;
  }
}

class SyncPeerTransport {
  SyncPeerTransport(this.endpoint, this.pin, List<int> key, this.vaultId)
    : key = Uint8List.fromList(key);
  HttpClient? _client;
  void close() {
    _client?.close(force: true);
    key.fillRange(0, key.length, 0);
  }

  final Uri endpoint;
  final String pin, vaultId;
  final Uint8List key;
  Future<Map<String, dynamic>> exchange(
    String path,
    String token,
    Map<String, dynamic> data,
  ) async {
    if (!localSyncEndpoint(endpoint)) {
      throw const FormatException('Sync endpoint is not on the local network');
    }
    final client = _client =
        HttpClient(context: SecurityContext(withTrustedRoots: false))
          ..connectionTimeout = const Duration(seconds: 6)
          ..autoUncompress = false
          ..badCertificateCallback = (cert, host, port) =>
              host == endpoint.host &&
              port == endpoint.port &&
              hashes.sha256.convert(cert.der).toString() == pin;
    try {
      final request = await client
          .postUrl(endpoint.replace(path: path))
          .timeout(const Duration(seconds: 8));
      request.followRedirects = false;
      request.headers.set(HttpHeaders.authorizationHeader, 'Bearer $token');
      request.headers.contentType = ContentType('application', 'octet-stream');
      final body = await sealSync(
        utf8.encode(jsonEncode(data)),
        key,
        'ArbiterSync/v1/$vaultId',
      );
      request.contentLength = body.length;
      request.add(body);
      final response = await request.close().timeout(
        const Duration(seconds: 25),
      );
      if (response.statusCode != 200) {
        throw HttpException(
          response.statusCode == 401
              ? 'Desktop authorization expired. Pair again.'
              : response.statusCode == 409
              ? 'Unlock your desktop vault and try again.'
              : 'Desktop sync returned ${response.statusCode}.',
        );
      }
      final bytes = BytesBuilder();
      await for (final part in response.timeout(const Duration(seconds: 15))) {
        bytes.add(part);
        if (bytes.length > maxSyncBytes + 28) {
          throw const FormatException('Sync message is too large');
        }
      }
      return jsonDecode(
        utf8.decode(
          await openSync(bytes.takeBytes(), key, 'ArbiterSync/v1/$vaultId'),
        ),
      ) as Map<String, dynamic>;
    } finally {
      close();
    }
  }
}
