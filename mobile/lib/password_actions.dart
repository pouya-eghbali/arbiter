import 'l10n.dart';

import 'dart:async';
import 'dart:convert';

import 'package:crypto/crypto.dart';

import 'dart:math';

import 'package:flutter/material.dart';
import 'package:local_auth/local_auth.dart';

import 'browser_model.dart';
import 'native_browser.dart';
import 'credential_vault.dart';

String generatePassword({int length = 20, bool symbols = true}) {
  if (length < 16 || length > 64) throw RangeError.range(length, 16, 64);
  final random = Random.secure();
  final groups = [
    'abcdefghijkmnopqrstuvwxyz',
    'ABCDEFGHJKLMNPQRSTUVWXYZ',
    '23456789',
    if (symbols) '!@#\$%&*+-=?',
  ];
  final alphabet = groups.join();
  final result = [
    for (final group in groups) group[random.nextInt(group.length)],
  ];
  while (result.length < length) {
    result.add(alphabet[random.nextInt(alphabet.length)]);
  }
  for (var i = result.length - 1; i > 0; i--) {
    final j = random.nextInt(i + 1), value = result[i];
    result[i] = result[j];
    result[j] = value;
  }
  return result.join();
}

class PasswordActions {
  PasswordActions(
    this.model,
    this.engine,
    this.context,
    this.changed, {
    this.verifier,
    this.openVault,
  });
  final Future<bool> Function()? verifier;
  final Future<CredentialVault> Function()? openVault;
  final BrowserModel model;
  final BrowserEngine engine;
  final BuildContext Function() context;
  final VoidCallback changed;
  CredentialVault? _vault;
  final Stopwatch _verifiedClock = Stopwatch();
  Timer? _expiry, _pendingExpiry;
  Route<dynamic>? _dialogRoute;
  int _epoch = 0;
  String? _lastFilled;
  String fingerprint(Map<String, dynamic> data) => sha256
      .convert(
        utf8.encode(
          jsonEncode([data['origin'], data['username'], data['password']]),
        ),
      )
      .toString();
  void sensitive(bool value) {
    unawaited(
      engine
          .command<void>('secureScreen', data: {'value': value})
          .catchError((Object _) {}),
    );
  }

  bool busy = false, disposed = false;
  Map<String, dynamic>? field, _pending;
  String? _pendingTab;
  DateTime? _submitted;
  bool get visible =>
      field != null &&
      field!["tab"] == model.activeTab.id &&
      model.suggestPasswords &&
      !(model.privateMode && model.privateLocked);
  void focus(String id, Map<String, dynamic> data) {
    if (!model.suggestPasswords ||
        id != model.activeTab.id ||
        Uri.tryParse(data['origin'] as String? ?? '')?.scheme != 'https') {
      return;
    }
    final current = Uri.tryParse(model.activeTab.url);
    if (current?.scheme != 'https' || current!.origin != data['origin']) return;
    field = {...data, "tab": id};
    changed();
  }

  void submitted(String id, Map<String, dynamic> data) {
    if (id != model.activeTab.id ||
        !model.savePasswords ||
        model.privateMode ||
        Uri.tryParse(data['origin'] as String? ?? '')?.scheme != 'https') {
      return;
    }
    final current = Uri.tryParse(model.activeTab.url);
    if (current?.scheme != 'https' || current!.origin != data['origin']) return;
    _pending?.clear();
    _pending = Map.from(data);
    _pendingTab = id;
    _submitted = DateTime.now();
    _pendingExpiry?.cancel();
    _pendingExpiry = Timer(const Duration(seconds: 45), () {
      _pending?.clear();
      _pending = null;
    });
  }

  Future<CredentialVault?> _unlock() async {
    final elapsed = _verifiedClock.isRunning
        ? _verifiedClock.elapsed.inSeconds
        : 999999;
    if (_vault != null && elapsed < model.passwordUnlockSeconds) return _vault;
    _vault?.dispose();
    _vault = null;
    engine.userVerificationActive = true;
    final epoch = _epoch;
    try {
      final approved =
          await (verifier?.call() ??
              LocalAuthentication().authenticate(
                localizedReason: tr('Unlock your saved passwords'),
                biometricOnly: false,
                persistAcrossBackgrounding: true,
              ));
      if (!approved) return null;
      if (disposed || epoch != _epoch) return null;
      final vault = await (openVault?.call() ?? CredentialVault.open());
      if (disposed || epoch != _epoch) {
        vault.dispose();
        return null;
      }
      _vault = vault;
      _verifiedClock
        ..reset()
        ..start();
      _expiry?.cancel();
      _expiry = Timer(
        Duration(
          seconds: model.passwordUnlockSeconds == 0
              ? 60
              : model.passwordUnlockSeconds,
        ),
        lock,
      );
      return vault;
    } finally {
      engine.userVerificationActive = false;
    }
  }

  void message(String text) {
    if (!disposed) {
      ScaffoldMessenger.of(context())
          .showSnackBar(SnackBar(content: UiText(text)));
    }
  }

  Future<void> fill() async {
    if (busy || field == null) return;
    busy = true;
    sensitive(true);
    final id = model.activeTab.id, origin = field!['origin'] as String;
    try {
      final vault = await _unlock();
      if (vault == null || disposed || id != model.activeTab.id) return;
      final entries = vault.state.visible
          .where(
            (v) =>
                v.value.data['kind'] == 'password' &&
                v.value.data['origin'] == origin,
          )
          .toList();
      if (entries.isEmpty) {
        message('No saved passwords for this website.');
        return;
      }
      final selected = await showModalBottomSheet<Map<String, dynamic>>(
        context: context(),
        isScrollControlled: true,
        builder: (c) {
          _dialogRoute = ModalRoute.of(c);
          return SafeArea(
            child: ConstrainedBox(
              constraints: BoxConstraints(
                maxHeight: MediaQuery.sizeOf(c).height * .55,
              ),
              child: ListView(
                shrinkWrap: true,
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 20),
                children: [
                  const Padding(
                    padding: EdgeInsets.all(12),
                    child: UiText(
                      'Choose an account',
                      style: TextStyle(fontSize: 22),
                    ),
                  ),
                  for (final e in entries)
                    ListTile(
                      leading: const Icon(Icons.key_rounded),
                      title: (e.value.data['username'] as String? ?? '').isEmpty
                          ? const UiText('Password')
                          : Text(e.value.data['username'] as String),
                      onTap: () => Navigator.pop(c, e.value.data),
                    ),
                ],
              ),
            ),
          );
        },
      );
      if (selected == null ||
          disposed ||
          id != model.activeTab.id ||
          _vault != vault) {
        return;
      }
      _lastFilled = fingerprint(selected);
      final filled = await engine.command<bool>(
        'fill',
        id: id,
        data: {
          'origin': origin,
          'username': selected['username'],
          'password': selected['password'],
        },
      );
      if (filled == true) {
        field = null;
        changed();
      } else {
        message('This form is no longer available.');
      }
    } catch (_) {
      message('Could not unlock or fill your saved password.');
    } finally {
      busy = false;
      sensitive(false);
      _dialogRoute = null;
      if (model.passwordUnlockSeconds == 0) lock();
    }
  }

  Future<void> generate() async {
    if (busy || field == null) return;
    busy = true;
    sensitive(true);
    try {
      final id = model.activeTab.id, origin = field!['origin'] as String;
      var length = 20, symbols = true, value = generatePassword();
      final selected = await showDialog<String>(
        context: context(),
        builder: (c) {
          _dialogRoute = ModalRoute.of(c);
          return StatefulBuilder(
            builder: (c, set) => AlertDialog(
              title: const UiText('Suggest a strong password'),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  SelectableText(
                    value,
                    style: const TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 19,
                    ),
                  ),
                  const SizedBox(height: 16),
                  UiText('{0} characters', values: [length.toString()]),
                  Slider(
                    value: length.toDouble(),
                    min: 16,
                    max: 64,
                    divisions: 48,
                    onChanged: (v) => set(() {
                      length = v.round();
                      value = generatePassword(
                        length: length,
                        symbols: symbols,
                      );
                    }),
                  ),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const UiText('Include symbols'),
                    value: symbols,
                    onChanged: (v) => set(() {
                      symbols = v;
                      value = generatePassword(
                        length: length,
                        symbols: symbols,
                      );
                    }),
                  ),
                  TextButton(
                    onPressed: () => set(
                      () => value = generatePassword(
                        length: length,
                        symbols: symbols,
                      ),
                    ),
                    child: const UiText('Generate another'),
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(c),
                  child: const UiText('Cancel'),
                ),
                FilledButton(
                  onPressed: () => Navigator.pop(c, value),
                  child: const UiText('Use password'),
                ),
              ],
            ),
          );
        },
      );
      if (selected == null || disposed || id != model.activeTab.id) return;
      final filled = await engine.command<bool>(
        'fill',
        id: id,
        data: {
          'origin': origin,
          'username': '',
          'password': selected,
          'generate': true,
        },
      );
      if (filled != true) message('This form is no longer available.');
    } finally {
      busy = false;
      sensitive(false);
      _dialogRoute = null;
    }
  }

  Future<void> finished(String id) async {
    final pending = _pending;
    if (!model.savePasswords || model.privateMode) {
      pending?.clear();
      _pending = null;
      _pendingExpiry?.cancel();
      return;
    }
    if (pending == null ||
        id != _pendingTab ||
        id != model.activeTab.id ||
        _submitted == null) {
      return;
    }
    if (DateTime.now().difference(_submitted!).inSeconds > 45) {
      _pending = null;
      return;
    }
    if (busy) {
      Future.delayed(const Duration(milliseconds: 500), () => finished(id));
      return;
    }
    if (await engine.command<bool>('hasPasswordField', id: id) != false) return;
    if (disposed || id != model.activeTab.id || !identical(pending, _pending)) {
      return;
    }
    _pending = null;
    _pendingExpiry?.cancel();
    _pendingTab = null;
    if (fingerprint(pending) == _lastFilled ||
        (_vault?.state.visible.any(
              (e) =>
                  e.value.data['kind'] == 'password' &&
                  fingerprint(e.value.data) == fingerprint(pending),
            ) ??
            false)) {
      pending.clear();
      return;
    }
    field = null;
    changed();
    final user = TextEditingController(
      text: pending['username'] as String? ?? '',
    );
    sensitive(true);
    try {
      final save = await showDialog<bool>(
        context: context(),
        builder: (c) {
          _dialogRoute = ModalRoute.of(c);
          return AlertDialog(
            title: const UiText('Save this password?'),
            content: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(pending['origin'] as String),
                const SizedBox(height: 14),
                TextField(
                  controller: user,
                  autocorrect: false,
                  decoration: InputDecoration(labelText: tr('Username')),
                ),
                const SizedBox(height: 12),
                const UiText(
                  'Save securely in your Arbiter vault and sync to your paired devices.',
                ),
              ],
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(c, false),
                child: const UiText('Not now'),
              ),
              FilledButton(
                onPressed: () => Navigator.pop(c, true),
                child: const UiText('Save'),
              ),
            ],
          );
        },
      );
      if (save != true || disposed) return;
      final vault = await _unlock();
      if (vault == null || disposed) return;
      await vault.putPassword(
        pending['origin'] as String,
        user.text,
        pending['password'] as String,
      );
      message('Password saved.');
    } catch (_) {
      message('Could not save this password.');
    } finally {
      pending.clear();
      sensitive(false);
      _dialogRoute = null;
      Future.delayed(const Duration(milliseconds: 400), user.dispose);
      if (model.passwordUnlockSeconds == 0) lock();
    }
  }

  void dismiss() {
    field = null;
    changed();
  }

  void lock() {
    _expiry?.cancel();
    _vault?.dispose();
    _vault = null;
    _verifiedClock.stop();
  }

  void background() {
    ++_epoch;
    lock();
    _pendingExpiry?.cancel();
    _lastFilled = null;
    if (_dialogRoute?.isCurrent == true) Navigator.of(context()).pop();
    _dialogRoute = null;
    field = null;
    _pending?.clear();
    _pending = null;
    changed();
  }

  void dispose() {
    disposed = true;
    ++_epoch;
    _expiry?.cancel();
    _pendingExpiry?.cancel();
    _vault?.dispose();
    _pending?.clear();
  }
}
