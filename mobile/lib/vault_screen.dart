import 'l10n.dart';

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:local_auth/local_auth.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

import 'browser_model.dart';
import 'browser_shell.dart' show Glyph, IconControl;
import 'credential_vault.dart';
import 'native_browser.dart';

class VaultScreen extends StatefulWidget {
  const VaultScreen({super.key, required this.model, required this.engine});
  final BrowserModel model;
  final BrowserEngine engine;
  @override
  State<VaultScreen> createState() => _VaultScreenState();
}

class _VaultScreenState extends State<VaultScreen> with WidgetsBindingObserver {
  CredentialVault? vault;
  bool busy = false, authenticating = false;
  String? error, status;
  String query = '';
  Timer? _expiry, _syncTicker;
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    unawaited(
      widget.engine.command<void>('secureScreen', data: {'value': true}),
    );
    WidgetsBinding.instance.addPostFrameCallback((_) => _unlock());
    _syncTicker = Timer.periodic(const Duration(seconds: 20), (_) {
      if (vault?.peer != null && !busy && !authenticating) {
        unawaited(_operation(() => vault!.sync(), 'Credentials synced.'));
      }
    });
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _expiry?.cancel();
    _syncTicker?.cancel();
    vault?.dispose();
    unawaited(
      widget.engine.command<void>('secureScreen', data: {'value': false}),
    );
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused ||
        state == AppLifecycleState.inactive && !authenticating) {
      _lock();
    }
  }

  void _lock() {
    _expiry?.cancel();
    vault?.dispose();
    if (mounted) {
      setState(() {
        vault = null;
        busy = false;
      });
    }
  }

  Future<void> _unlock() async {
    if (busy || authenticating || widget.engine.userVerificationActive) return;
    setState(() {
      authenticating = true;
      error = null;
    });
    widget.engine.userVerificationActive = true;
    try {
      final allowed = await LocalAuthentication().authenticate(
        localizedReason: tr('Unlock your Arbiter passwords and passkeys'),
        biometricOnly: false,
        persistAcrossBackgrounding: true,
      );
      if (!allowed ||
          !mounted ||
          WidgetsBinding.instance.lifecycleState == AppLifecycleState.paused) {
        return;
      }
      final loaded = await CredentialVault.open();
      if (!mounted) {
        loaded.dispose();
        return;
      }
      setState(() => vault = loaded);
      _expiry = Timer(const Duration(minutes: 5), _lock);
    } catch (_) {
      if (mounted) {
        setState(
          () => error = 'Could not unlock the vault. Make sure this device has a passcode or biometrics configured.',
        );
      }
    } finally {
      widget.engine.userVerificationActive = false;
      if (mounted) setState(() => authenticating = false);
    }
  }

  Future<void> _operation(
    Future<void> Function() action,
    String success,
  ) async {
    final session = vault;
    if (session == null || busy) return;
    if (busy) return;
    setState(() {
      busy = true;
      error = null;
      status = null;
    });
    try {
      await action();
      if (mounted && identical(vault, session)) {
        setState(() => status = success);
      }
    } on TimeoutException {
      if (mounted && identical(vault, session)) {
        setState(
          () => error = 'Waiting for your desktop. Open Arbiter on the same Wi-Fi and unlock its sync vault.',
        );
      }
    } catch (e) {
      if (mounted && identical(vault, session)) {
        setState(
          () => error = e is FormatException
              ? e.message
              : e.toString().contains('SocketException')
              ? 'Desktop not reachable. Keep both apps open on the same Wi-Fi.'
              : e.toString().contains('HandshakeException')
              ? 'The desktop certificate did not match. Scan a new pairing code from your desktop.'
              : 'Sync could not finish. Your local credentials have been kept.',
        );
      }
    } finally {
      if (mounted && identical(vault, session)) setState(() => busy = false);
    }
  }

  Future<void> _pair({bool camera = true}) async {
    String? code;
    if (camera) {
      authenticating = true; // The camera permission dialog must not discard the unlocked vault.
      code = await Navigator.of(
        context,
      ).push<String>(MaterialPageRoute(builder: (_) => const PairingScanner()));
      authenticating = false;
    } else {
      final input = TextEditingController();
      code = await showDialog<String>(
        context: context,
        builder: (c) => AlertDialog(
          title: const UiText('Paste desktop pairing code'),
          content: TextField(
            controller: input,
            maxLines: 4,
            autocorrect: false,
            decoration: InputDecoration(hintText: tr('arbiter-pair://v1…')),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(c),
              child: const UiText('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(c, input.text),
              child: const UiText('Continue'),
            ),
          ],
        ),
      );
      Future.delayed(const Duration(milliseconds: 400), input.dispose);
    }
    if (!mounted || code == null || vault == null) return;
    try {
      final info = PairingInfo.parse(code);
      final allowed = await showDialog<bool>(
        context: context,
        builder: (c) => AlertDialog(
          title: UiText("Pair with {0}?", values: [(info.name).toString()]),
          content: UiText(
            "This shares your Arbiter passwords and passkeys with this desktop. Only continue if you opened this code yourself in Arbiter.\n\nLocal address: {0}\nCertificate: {1}…",
            values: [
              (info.endpoint.host).toString(),
              (info.pin.substring(0, 16)).toString(),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(c, false),
              child: const UiText('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(c, true),
              child: const UiText('Pair devices'),
            ),
          ],
        ),
      );
      if (allowed == true && vault != null) {
        await _operation(
          () => vault!.pair(
            info,
            'Arbiter ${Theme.of(context).platform == TargetPlatform.iOS ? 'iPhone' : 'Android'}',
          ),
          'Devices paired and credentials synced.',
        );
      }
    } on FormatException catch (e) {
      setState(() => error = e.message);
    } catch (_) {
      setState(() => error = 'This is not a valid Arbiter pairing code.');
    }
  }

  Future<void> _add() async {
    final website = TextEditingController(),
        username = TextEditingController(),
        password = TextEditingController();
    final accepted = await showDialog<bool>(
      context: context,
      builder: (c) => AlertDialog(
        title: const UiText('Save a password'),
        content: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: website,
                keyboardType: TextInputType.url,
                autocorrect: false,
                decoration: InputDecoration(labelText: tr('Website')),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: username,
                autocorrect: false,
                decoration: InputDecoration(labelText: tr('Username')),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: password,
                obscureText: true,
                autocorrect: false,
                enableSuggestions: false,
                decoration: InputDecoration(labelText: tr('Password')),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(c),
            child: const UiText('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(c, true),
            child: const UiText('Save'),
          ),
        ],
      ),
    );
    if (accepted == true && vault != null) {
      await _operation(() async {
        final uri = addressUri(website.text, widget.model.searchEngine);
        if (!website.text.contains('.') && !website.text.contains('://')) {
          throw const FormatException('Enter a website address');
        }
        await vault!.putPassword(uri.origin, username.text, password.text);
      }, 'Password saved on this device.');
    }
    Future.delayed(const Duration(milliseconds: 400), () {
      website.dispose();
      username.dispose();
      password.dispose();
    });
  }

  Future<void> _record(String id, CredentialVersion record) async {
    final data = record.data, isPassword = record.data['kind'] == 'password';
    final origin = data[isPassword ? 'origin' : 'rpId'] as String? ?? '';
    final action = await showModalBottomSheet<String>(
      context: context,
      builder: (c) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(22, 0, 22, 20),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(origin, style: const TextStyle(fontSize: 23)),
              const SizedBox(height: 8),
              Text(data['username'] as String? ?? ''),
              const SizedBox(height: 16),
              if (isPassword) ...[
                ListTile(
                  leading: const Glyph('copy'),
                  title: const UiText('Copy password'),
                  onTap: () => Navigator.pop(c, 'copy'),
                ),
                if (widget.model.activeTab.url.isNotEmpty &&
                    Uri.tryParse(widget.model.activeTab.url)?.origin == origin)
                  ListTile(
                    leading: const Glyph('key-round'),
                    title: const UiText('Fill in current page'),
                    onTap: () => Navigator.pop(c, 'fill'),
                  ),
              ],
              ListTile(
                leading: const Glyph('trash-2'),
                title: const UiText('Delete credential'),
                onTap: () => Navigator.pop(c, 'delete'),
              ),
            ],
          ),
        ),
      ),
    );
    if (vault == null) return;
    switch (action) {
      case 'copy':
        await Clipboard.setData(
          ClipboardData(text: data['password'] as String),
        );
      case 'fill':
        final filled = await widget.engine.command<bool>(
          'fill',
          id: widget.model.activeTab.id,
          data: {
            'origin': origin,
            'username': data['username'],
            'password': data['password'],
          },
        );
        if (!mounted) return;
        if (filled == true) {
          Navigator.pop(context);
        } else {
          setState(
            () =>
                error = 'No matching password form is available on this page.',
          );
        }
      case 'delete':
        if (!mounted) return;
        final confirmed = await showDialog<bool>(
          context: context,
          builder: (c) => AlertDialog(
            title: const UiText('Delete credential?'),
            content: const UiText(
              'This deletion will also sync to your paired devices.',
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(c),
                child: const UiText('Cancel'),
              ),
              FilledButton(
                onPressed: () => Navigator.pop(c, true),
                child: const UiText('Delete'),
              ),
            ],
          ),
        );
        if (confirmed == true && vault != null) {
          await _operation(() => vault!.remove(id), 'Credential deleted.');
        }
    }
  }

  Future<void> _resolve(String id) async {
    final choices = vault?.state.records[id] ?? [];
    final chosen = await showDialog<CredentialVersion>(
      context: context,
      builder: (c) => SimpleDialog(
        title: const UiText('Resolve conflicting changes'),
        children: [
          const Padding(
            padding: EdgeInsets.all(20),
            child: UiText(
              'Both versions were kept. Choose which change to keep on your devices.',
            ),
          ),
          for (final version in choices)
            SimpleDialogOption(
              onPressed: () => Navigator.pop(c, version),
              child: Text(
                version.deleted
                    ? 'Keep deletion'
                    : '${version.data['origin'] ?? version.data['rpId']}\n${version.data['username'] ?? ''} · ${version.clock.keys.join(', ').substring(0, 8)}',
              ),
            ),
        ],
      ),
    );
    if (chosen != null && vault != null) {
      await _operation(
        () => vault!.resolve(id, chosen),
        'Conflict resolved. Sync to apply it to the other device.',
      );
    }
  }

  @override
  Widget build(BuildContext c) {
    final entries =
        vault?.state.visible
            .where(
              (e) =>
                  '${e.value.data['origin']} ${e.value.data['rpId']} ${e.value.data['username']}'
                      .toLowerCase()
                      .contains(query.toLowerCase()),
            )
            .toList() ??
        [];
    return Scaffold(
      appBar: AppBar(
        title: const UiText('Passwords & passkeys'),
        actions: [
          if (vault != null)
            IconControl('plus', 'Save a password', busy ? null : _add),
          if (vault != null) IconControl('lock', 'Lock vault', _lock),
        ],
      ),
      body: vault == null
          ? Center(
              child: Padding(
                padding: const EdgeInsets.all(28),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const Glyph('key-round', size: 44),
                    const SizedBox(height: 20),
                    const UiText(
                      'Your keys, close at hand',
                      style: TextStyle(fontSize: 25),
                    ),
                    const SizedBox(height: 12),
                    const UiText(
                      'Credentials are encrypted on this device. Unlock using your biometrics or device passcode.',
                      textAlign: TextAlign.center,
                    ),
                    if (error != null)
                      Padding(
                        padding: const EdgeInsets.only(top: 18),
                        child: UiText(error!, textAlign: TextAlign.center),
                      ),
                    const SizedBox(height: 22),
                    FilledButton(
                      onPressed: authenticating ? null : _unlock,
                      child: const UiText('Unlock vault'),
                    ),
                  ],
                ),
              ),
            )
          : Column(
              children: [
                if (busy) const LinearProgressIndicator(),
                Padding(
                  padding: const EdgeInsets.fromLTRB(18, 0, 18, 12),
                  child: TextField(
                    decoration: InputDecoration(
                      hintText: tr('Search saved credentials'),
                      prefixIcon: Icon(Icons.search),
                    ),
                    onChanged: (s) => setState(() => query = s),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 18),
                  child: Align(
                    alignment: Alignment.centerLeft,
                    child: TextButton.icon(
                      icon: const Glyph('key-round', size: 18),
                      label: UiText(
                        Theme.of(c).platform == TargetPlatform.iOS
                            ? 'Enable system passkeys & AutoFill'
                            : 'Enable system passkeys',
                      ),
                      onPressed: () async {
                        final enabled = await widget.engine.command<bool>(
                          'enableCredentialProvider',
                        );
                        if (mounted && enabled != true) {
                          setState(
                            () => error = 'System passkey providers require Android 14 or newer, or iOS 17 or newer. Check your device’s password provider settings.',
                          );
                        }
                      },
                    ),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 18),
                  child: Card(
                    elevation: 0,
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Row(
                            children: [
                              const Glyph('link'),
                              const SizedBox(width: 10),
                              Expanded(
                                child: Text(
                                  vault!.peer == null
                                      ? 'Sync with your desktop'
                                      : vault!.peer!['name'] as String,
                                  style: Theme.of(c).textTheme.titleMedium,
                                ),
                              ),
                              if (vault!.peer != null)
                                IconControl(
                                  'refresh-cw',
                                  'Sync now',
                                  busy
                                      ? null
                                      : () => _operation(
                                          () => vault!.sync(),
                                          'Credentials synced.',
                                        ),
                                ),
                            ],
                          ),
                          const SizedBox(height: 8),
                          const UiText(
                            'Direct encrypted sync over the same Wi-Fi. Keep Arbiter open on your desktop and its sync unlocked.',
                          ),
                          Wrap(
                            spacing: 8,
                            children: [
                              TextButton.icon(
                                onPressed: busy ? null : _pair,
                                icon: const Glyph('qr-code', size: 19),
                                label: const UiText('Scan desktop code'),
                              ),
                              TextButton(
                                onPressed: busy
                                    ? null
                                    : () => _pair(camera: false),
                                child: const UiText('Paste code'),
                              ),
                              if (vault!.peer != null)
                                TextButton(
                                  onPressed: busy
                                      ? null
                                      : () async {
                                          final confirmed =
                                              await showDialog<bool>(
                                                context: context,
                                                builder: (c) => AlertDialog(
                                                  title: const UiText(
                                                    'Disconnect sync?',
                                                  ),
                                                  content: const UiText(
                                                    'Credentials stay on this device. Revoke this phone in desktop Sync settings to remove its access there too.',
                                                  ),
                                                  actions: [
                                                    TextButton(
                                                      onPressed: () =>
                                                          Navigator.pop(c),
                                                      child: const UiText(
                                                        'Cancel',
                                                      ),
                                                    ),
                                                    FilledButton(
                                                      onPressed: () =>
                                                          Navigator.pop(
                                                            c,
                                                            true,
                                                          ),
                                                      child: const UiText(
                                                        'Disconnect',
                                                      ),
                                                    ),
                                                  ],
                                                ),
                                              );
                                          if (confirmed == true &&
                                              vault != null) {
                                            await _operation(
                                              () => vault!.disconnect(),
                                              'Sync disconnected.',
                                            );
                                          }
                                        },
                                  child: const UiText('Disconnect'),
                                ),
                            ],
                          ),
                          if (error != null)
                            Padding(
                              padding: const EdgeInsets.only(top: 10),
                              child: Text(
                                error!,
                                style: TextStyle(
                                  color: Theme.of(c).colorScheme.error,
                                ),
                              ),
                            ),
                          if (status != null)
                            Padding(
                              padding: const EdgeInsets.only(top: 10),
                              child: UiText(status!),
                            ),
                        ],
                      ),
                    ),
                  ),
                ),
                Expanded(
                  child: ListView(
                    children: [
                      for (final id in vault!.state.conflicts)
                        ListTile(
                          leading: const Glyph('info'),
                          title: const UiText('Conflicting credential changes'),
                          subtitle: const UiText(
                            'Both versions have been kept',
                          ),
                          onTap: () => _resolve(id),
                        ),
                      if (entries.isEmpty)
                        const Padding(
                          padding: EdgeInsets.all(36),
                          child: UiText(
                            'No saved credentials yet. Add a password or pair with your desktop.',
                            textAlign: TextAlign.center,
                          ),
                        ),
                      for (final e in entries)
                        ListTile(
                          leading: Glyph(
                            e.value.data['kind'] == 'passkey'
                                ? 'fingerprint'
                                : 'key-round',
                          ),
                          title: Text(
                            e.value.data['origin'] as String? ??
                                e.value.data['rpId'] as String? ??
                                '',
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                          subtitle: Text(
                            e.value.data['username'] as String? ?? '',
                          ),
                          trailing: const Glyph('chevron-right', size: 18),
                          onTap: () => _record(e.key, e.value),
                        ),
                    ],
                  ),
                ),
              ],
            ),
    );
  }
}

class PairingScanner extends StatefulWidget {
  const PairingScanner({super.key});
  @override
  State<PairingScanner> createState() => _PairingScannerState();
}

class _PairingScannerState extends State<PairingScanner> {
  final scanner = MobileScannerController(
    formats: [BarcodeFormat.qrCode],
    detectionSpeed: DetectionSpeed.noDuplicates,
  );
  bool done = false;
  @override
  void dispose() {
    unawaited(scanner.dispose());
    super.dispose();
  }

  @override
  Widget build(BuildContext c) => Scaffold(
    appBar: AppBar(title: const UiText('Pair with desktop')),
    body: Stack(
      children: [
        MobileScanner(
          controller: scanner,
          onDetect: (capture) {
            for (final barcode in capture.barcodes) {
              final value = barcode.rawValue;
              if (done ||
                  value == null ||
                  !value.startsWith('arbiter-pair://')) {
                continue;
              }
              done = true;
              Navigator.pop(c, value);
              break;
            }
          },
        ),
        Align(
          alignment: Alignment.bottomCenter,
          child: SafeArea(
            child: Container(
              margin: const EdgeInsets.all(24),
              padding: const EdgeInsets.all(20),
              decoration: BoxDecoration(
                color: Colors.black87,
                borderRadius: BorderRadius.circular(24),
              ),
              child: const UiText(
                'On your desktop, open Settings → Sync and choose Pair a phone. Keep the code private.',
                style: TextStyle(color: Colors.white),
                textAlign: TextAlign.center,
              ),
            ),
          ),
        ),
      ],
    ),
  );
}
