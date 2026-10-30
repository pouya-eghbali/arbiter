import 'dart:async';
import 'dart:ui';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:local_auth/local_auth.dart';
import 'package:share_plus/share_plus.dart';
import 'package:url_launcher/url_launcher.dart';

import 'browser_model.dart';
import 'native_browser.dart';
import 'vault_screen.dart';
import 'credential_vault.dart';

const favorites = [
  ('Wikipedia', 'https://www.wikipedia.org', 'W'),
  ('GitHub', 'https://github.com', 'G'),
  ('YouTube', 'https://www.youtube.com', '▶'),
  ('Reddit', 'https://www.reddit.com', 'r'),
  ('Hacker News', 'https://news.ycombinator.com', 'Y'),
  ('MDN', 'https://developer.mozilla.org', 'M'),
];

class Glyph extends StatelessWidget {
  const Glyph(this.name, {super.key, this.size = 22, this.color});
  final String name;
  final double size;
  final Color? color;
  @override
  Widget build(BuildContext context) => SvgPicture.asset(
    'assets/icons/$name.svg',
    width: size,
    height: size,
    colorFilter: ColorFilter.mode(
      color ?? Theme.of(context).colorScheme.onSurface,
      BlendMode.srcIn,
    ),
  );
}

class IconControl extends StatelessWidget {
  const IconControl(
    this.icon,
    this.label,
    this.onPressed, {
    super.key,
    this.color,
  });
  final String icon, label;
  final VoidCallback? onPressed;
  final Color? color;
  @override
  Widget build(BuildContext context) => IconButton(
    tooltip: label,
    onPressed: onPressed,
    color: color,
    icon: Glyph(
      icon,
      color: onPressed == null ? Theme.of(context).disabledColor : color,
    ),
  );
}

class Glass extends StatelessWidget {
  const Glass({super.key, required this.child, this.radius = 30});
  final Widget child;
  final double radius;
  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return DecoratedBox(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(radius),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: .20),
            blurRadius: 26,
            offset: const Offset(0, 8),
          ),
        ],
      ),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(radius),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 22, sigmaY: 22),
          child: ColoredBox(
            color: scheme.surface.withValues(alpha: .88),
            child: child,
          ),
        ),
      ),
    );
  }
}

class BrowserShell extends StatefulWidget {
  const BrowserShell({super.key, required this.model});
  final BrowserModel model;
  @override
  State<BrowserShell> createState() => _BrowserShellState();
}

class _BrowserShellState extends State<BrowserShell>
    with WidgetsBindingObserver {
  BrowserModel get model => widget.model;
  late final BrowserEngine engine;
  Timer? _idle;
  bool overview = false, _authenticating = false;
  double _dragX = 0, _dragY = 0;
  Timer? _vaultSyncTimer;
  bool _syncingVault = false;
  CredentialVault? _syncVault;
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _vaultSyncTimer = Timer.periodic(
      const Duration(seconds: 20),
      (_) => _syncVaultInForeground(),
    );
    engine = BrowserEngine()
      ..onEvent = _event
      ..onRequest = _request;
    unawaited(_ready());
    _touch();
  }

  Future<void> _ready() async {
    try {
      final initial = await engine.command<String>('ready');
      if (initial != null && isWebAddress(initial)) _navigate(initial);
    } on MissingPluginException {
      /* Widget tests run without native engine views. */
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _vaultSyncTimer?.cancel();
    _syncVault?.dispose();
    _idle?.cancel();
    engine.dispose();
    super.dispose();
  }

  void _touch() {
    _idle?.cancel();
    if (model.privateLockSeconds > 0) {
      _idle = Timer(Duration(seconds: model.privateLockSeconds), _lock);
    }
  }

  Future<void> _syncVaultInForeground() async {
    if (!mounted ||
        _syncingVault ||
        WidgetsBinding.instance.lifecycleState != AppLifecycleState.resumed) {
      return;
    }
    _syncingVault = true;
    try {
      final vault = _syncVault = await CredentialVault.openExistingForSync();
      if (!mounted ||
          WidgetsBinding.instance.lifecycleState != AppLifecycleState.resumed) {
        vault?.dispose();
        return;
      }
      // Pairing is the opt-in. This only moves encrypted credentials to the
      // approved, pinned peer; displaying or filling still requires verification.
      if (vault?.peer != null) await vault!.sync();
    } catch (_) {
      /* Offline/locked peers are retried while the app is open. */
    } finally {
      _syncVault?.dispose();
      _syncVault = null;
      _syncingVault = false;
    }
  }

  void _lock() {
    if (!model.tabs.any((t) => t.isPrivate)) return;
    model.privateLocked = true;
    if (model.privateMode && mounted) {
      Navigator.of(context).popUntil((r) => r.isFirst);
    }
    unawaited(engine.command<void>('pausePrivate'));
    model.changed();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.inactive ||
        state == AppLifecycleState.paused) {
      _syncVault?.dispose();
      if (state == AppLifecycleState.paused || !engine.userVerificationActive) {
        _lock();
      }
      unawaited(model.flush());
    }
    if (state == AppLifecycleState.resumed) _touch();
  }

  void _event(String id, Map<String, dynamic> data) {
    if (!mounted) return;
    if (data['type'] == 'activity') {
      _touch();
      return;
    }
    if (data['type'] == 'download') {
      model.downloadEvent(data);
      return;
    }
    if (data['type'] == 'newTab' || data['type'] == 'openUrl') {
      final url = data['url'] as String? ?? '';
      if (isWebAddress(url)) {
        model.addTab(url: url);
        setState(() => overview = false);
      }
      return;
    }
    model.engineEvent(id, data);
  }

  Future<dynamic> _request(Map<String, dynamic> data) async {
    if (!mounted || model.privateMode && model.privateLocked) return false;
    if (data['request'] == 'external') {
      final uri = Uri.tryParse(data['url'] as String? ?? '');
      if (uri == null ||
          !const ['mailto', 'tel', 'sms', 'geo'].contains(uri.scheme)) {
        return false;
      }
      if (await _confirm('Open another app?', uri.toString())) {
        await launchUrl(uri, mode: LaunchMode.externalApplication);
      }
      return false;
    }
    if (data['request'] == 'permission') {
      return _confirm(
        'Allow ${data['kind']}?',
        '${data['origin']} wants access to ${data['kind']}.',
      );
    }
    if (data['request'] == 'download') {
      return _confirm(
        'Download file?',
        data['name'] as String? ?? 'Save this file on this device?',
      );
    }
    if (data['request'] == 'prompt') {
      return _textDialog(
        data['title'] as String? ?? 'Message from this page',
        initial: data['value'] as String? ?? '',
      );
    }
    if (data['request'] == 'confirm') {
      return _confirm(
        'Message from this page',
        data['message'] as String? ?? '',
      );
    }
    if (data['request'] == 'alert') {
      await showDialog<void>(
        context: context,
        builder: (c) => AlertDialog(
          title: const Text('Message from this page'),
          content: Text(data['message'] as String? ?? ''),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(c),
              child: const Text('OK'),
            ),
          ],
        ),
      );
    }
    return null;
  }

  Future<bool> _confirm(String title, String body) async =>
      await showDialog<bool>(
        context: context,
        builder: (c) => AlertDialog(
          title: Text(title),
          content: Text(body),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(c, false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(c, true),
              child: const Text('Allow'),
            ),
          ],
        ),
      ) ??
      false;
  Future<String?> _textDialog(String title, {String initial = ''}) async {
    final input = TextEditingController(text: initial);
    final value = await showDialog<String>(
      context: context,
      builder: (c) => AlertDialog(
        title: Text(title),
        content: TextField(
          controller: input,
          autofocus: true,
          maxLength: 80,
          onSubmitted: (s) => Navigator.pop(c, s.trim()),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(c),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(c, input.text.trim()),
            child: const Text('Save'),
          ),
        ],
      ),
    );
    // Dialogs animate out after their Future completes.
    Future.delayed(const Duration(milliseconds: 400), input.dispose);
    return value == null || value.isEmpty ? null : value;
  }

  Future<void> _unlock() async {
    if (_authenticating) return;
    _authenticating = true;
    try {
      final allowed = await LocalAuthentication().authenticate(
        localizedReason: 'Unlock private browsing',
        biometricOnly: false,
        persistAcrossBackgrounding: true,
      );
      if (allowed && mounted) {
        model.privateLocked = false;
        model.changed();
        _touch();
      }
    } catch (_) {
      if (mounted) {
        _message(
          'Set up a device passcode or biometrics to unlock private browsing.',
        );
      }
    } finally {
      _authenticating = false;
    }
  }

  void _message(String message) =>
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(message)));
  void _navigate(String value) {
    try {
      model.navigate(value);
      unawaited(engine.load(model.activeTab));
      setState(() => overview = false);
      _touch();
    } on FormatException catch (e) {
      _message(e.message);
    }
  }

  Future<void> _address() async {
    _touch();
    final value = await showModalBottomSheet<String>(
      context: context,
      isScrollControlled: true,
      builder: (c) => AddressSheet(model: model),
    );
    if (value != null && mounted) _navigate(value);
  }

  Future<void> _tabs() async {
    _touch();
    unawaited(
      engine.capture(model.activeTab).then((_) {
        if (mounted) setState(() {});
      }),
    );
    setState(() => overview = true);
  }

  Future<void> _select(BrowserTab tab) async {
    if (tab.id != model.activeTab.id) {
      unawaited(engine.capture(model.activeTab));
    }
    model.selectTab(tab);
    setState(() => overview = false);
    _touch();
  }

  void _close(BrowserTab tab) {
    unawaited(engine.close(tab));
    model.closeTab(tab);
    _touch();
  }

  void _newTab({bool? private}) {
    model.addTab(private: private);
    setState(() => overview = false);
    _touch();
  }

  Future<void> _workspaces() async {
    await showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      builder: (c) => WorkspaceSheet(
        model: model,
        onAdd: () async {
          Navigator.pop(c);
          final input = TextEditingController();
          bool isolated = false;
          final value = await showDialog<String>(
            context: context,
            builder: (d) => StatefulBuilder(
              builder: (d, update) => AlertDialog(
                title: const Text('New workspace'),
                content: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    TextField(
                      controller: input,
                      autofocus: true,
                      maxLength: 60,
                      decoration: const InputDecoration(hintText: 'Name'),
                    ),
                    SwitchListTile(
                      contentPadding: EdgeInsets.zero,
                      title: const Text('Separate sign-in session'),
                      value: isolated,
                      onChanged: (v) => update(() => isolated = v),
                    ),
                  ],
                ),
                actions: [
                  TextButton(
                    onPressed: () => Navigator.pop(d),
                    child: const Text('Cancel'),
                  ),
                  FilledButton(
                    onPressed: () {
                      if (input.text.trim().isNotEmpty) {
                        Navigator.pop(d, input.text.trim());
                      }
                    },
                    child: const Text('Create'),
                  ),
                ],
              ),
            ),
          );
          Future.delayed(const Duration(milliseconds: 400), input.dispose);
          if (value != null) model.addWorkspace(value, isolated: isolated);
        },
        onRename: (w) async {
          Navigator.pop(c);
          final name = await _textDialog('Rename workspace', initial: w.name);
          if (name != null) model.renameWorkspace(w, name);
        },
      ),
    );
  }

  Future<void> _share() async {
    final tab = model.activeTab;
    if (tab.url.isEmpty) return;
    final box = context.findRenderObject() as RenderBox?;
    await SharePlus.instance.share(
      ShareParams(
        text: tab.url,
        subject: tab.title,
        sharePositionOrigin: box == null
            ? null
            : box.localToGlobal(Offset.zero) & box.size,
      ),
    );
  }

  Future<void> _more() async {
    _touch();
    final action = await showModalBottomSheet<String>(
      context: context,
      builder: (c) {
        final tab = model.activeTab;
        return SafeArea(
          child: SingleChildScrollView(
            padding: const EdgeInsets.fromLTRB(20, 0, 20, 16),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceAround,
                  children: [
                    _menuAction(
                      c,
                      'share',
                      'Share',
                      tab.url.isEmpty ? null : 'share',
                    ),
                    _menuAction(
                      c,
                      tab.loading ? 'x' : 'refresh-cw',
                      tab.loading ? 'Stop' : 'Reload',
                      tab.url.isEmpty ? null : 'reload',
                    ),
                    _menuAction(
                      c,
                      'search',
                      'Find',
                      tab.url.isEmpty ? null : 'find',
                    ),
                  ],
                ),
                const SizedBox(height: 14),
                _row(
                  'layers',
                  'Workspaces',
                  () => Navigator.pop(c, 'workspaces'),
                ),
                _row('history', 'History', () => Navigator.pop(c, 'history')),
                _row(
                  'download',
                  'Downloads',
                  () => Navigator.pop(c, 'downloads'),
                ),
                _row(
                  'key-round',
                  'Passwords & passkeys',
                  () => Navigator.pop(c, 'vault'),
                ),
                _row(
                  'eye-off',
                  'New private tab',
                  () => Navigator.pop(c, 'private'),
                ),
                if (model.privateMode)
                  _row(
                    'lock',
                    'Lock private tabs',
                    () => Navigator.pop(c, 'lock'),
                  ),
                _row(
                  'settings',
                  'Settings',
                  () => Navigator.pop(c, 'settings'),
                ),
                if (tab.url.isNotEmpty)
                  _row('copy', 'Copy link', () => Navigator.pop(c, 'copy')),
              ],
            ),
          ),
        );
      },
    );
    if (!mounted || action == null) return;
    switch (action) {
      case 'share':
        await _share();
      case 'reload':
        await engine.command<void>(
          model.activeTab.loading ? 'stop' : 'reload',
          id: model.activeTab.id,
        );
      case 'find':
        await _find();
      case 'history':
        await _history();
      case 'downloads':
        await _downloads();
      case 'workspaces':
        await _workspaces();
      case 'vault':
        await Navigator.of(context).push(
          MaterialPageRoute<void>(
            builder: (_) => VaultScreen(model: model, engine: engine),
          ),
        );
      case 'private':
        _newTab(private: true);
      case 'lock':
        _lock();
      case 'settings':
        await Navigator.of(context).push(
          MaterialPageRoute<void>(
            builder: (_) => SettingsScreen(model: model, engine: engine),
          ),
        );
      case 'copy':
        await Clipboard.setData(ClipboardData(text: model.activeTab.url));
        _message('Link copied');
    }
  }

  Widget _menuAction(
    BuildContext c,
    String icon,
    String label,
    String? action,
  ) => Column(
    children: [
      IconButton.filledTonal(
        onPressed: action == null ? null : () => Navigator.pop(c, action),
        icon: Glyph(icon),
      ),
      Text(label, style: Theme.of(c).textTheme.labelMedium),
    ],
  );
  Widget _row(String icon, String label, VoidCallback onTap) => ListTile(
    contentPadding: const EdgeInsets.symmetric(horizontal: 10),
    leading: Glyph(icon),
    title: Text(label),
    trailing: const Glyph('chevron-right', size: 18),
    onTap: onTap,
  );
  Future<void> _find() async {
    await showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      builder: (c) => Padding(
        padding: EdgeInsets.only(
          left: 18,
          right: 18,
          bottom: MediaQuery.viewInsetsOf(c).bottom + 18,
        ),
        child: TextField(
          autofocus: true,
          decoration: const InputDecoration(
            hintText: 'Find in page',
            prefixIcon: Icon(Icons.search),
          ),
          onChanged: (q) => engine.command<void>(
            'find',
            id: model.activeTab.id,
            data: {'query': q},
          ),
          onSubmitted: (q) => engine.command<void>(
            'find',
            id: model.activeTab.id,
            data: {'query': q},
          ),
        ),
      ),
    );
    await engine.command<void>(
      'find',
      id: model.activeTab.id,
      data: {'query': ''},
    );
  }

  Future<void> _history() async {
    final chosen = await Navigator.of(context).push<String>(
      MaterialPageRoute(builder: (_) => HistoryScreen(model: model)),
    );
    if (chosen != null && mounted) _navigate(chosen);
  }

  Future<void> _downloads() => Navigator.of(context).push<void>(
    MaterialPageRoute(builder: (_) => DownloadsScreen(model: model)),
  );
  Future<void> _back() async {
    if (overview) {
      setState(() => overview = false);
      return;
    }
    if (model.activeTab.canBack) {
      await engine.command<void>('back', id: model.activeTab.id);
      return;
    }
    if (model.visibleTabs.length > 1) {
      _close(model.activeTab);
      return;
    }
    await SystemNavigator.pop();
  }

  @override
  Widget build(BuildContext context) {
    final tab = model.activeTab;
    final scheme = Theme.of(context).colorScheme;
    unawaited(
      engine.privateVisible(model.privateMode).catchError((Object _) {}),
    );
    final locked = model.privateMode && model.privateLocked;
    return AnnotatedRegion<SystemUiOverlayStyle>(
      value: scheme.brightness == Brightness.dark
          ? SystemUiOverlayStyle.light.copyWith(
              systemNavigationBarColor: Colors.transparent,
            )
          : SystemUiOverlayStyle.dark.copyWith(
              systemNavigationBarColor: Colors.transparent,
            ),
      child: PopScope(
        canPop: false,
        onPopInvokedWithResult: (didPop, _) {
          if (!didPop) _back();
        },
        child: Listener(
          onPointerDown: (_) => _touch(),
          child: Scaffold(
            resizeToAvoidBottomInset: true,
            body: SafeArea(
              bottom: false,
              child: Stack(
                children: [
                  Positioned.fill(
                    child: locked
                        ? _privateShield(context)
                        : overview
                        ? TabsOverview(
                            model: model,
                            onSelect: _select,
                            onClose: _close,
                            onWorkspaces: _workspaces,
                          )
                        : tab.url.isEmpty
                        ? _start(context)
                        : Stack(
                            children: [
                              Positioned.fill(
                                child: BrowserSurface(
                                  key: ValueKey(tab.id),
                                  tab: tab,
                                  workspace: model.workspace,
                                  privateContext: model.privateContextFor(
                                    model.workspace,
                                  ),
                                ),
                              ),
                              if (tab.error != null)
                                Positioned.fill(
                                  child: ColoredBox(
                                    color: scheme.surface,
                                    child: Center(
                                      child: Padding(
                                        padding: const EdgeInsets.all(32),
                                        child: Column(
                                          mainAxisSize: MainAxisSize.min,
                                          children: [
                                            const Glyph('globe', size: 42),
                                            const SizedBox(height: 20),
                                            const Text(
                                              'This page could not be loaded',
                                              style: TextStyle(fontSize: 22),
                                            ),
                                            const SizedBox(height: 12),
                                            Text(
                                              tab.error!,
                                              textAlign: TextAlign.center,
                                            ),
                                            const SizedBox(height: 24),
                                            FilledButton(
                                              onPressed: () =>
                                                  _navigate(tab.url),
                                              child: const Text('Try again'),
                                            ),
                                          ],
                                        ),
                                      ),
                                    ),
                                  ),
                                ),
                            ],
                          ),
                  ),
                  if (!locked && MediaQuery.viewInsetsOf(context).bottom == 0)
                    Positioned(
                      left: 18,
                      right: 18,
                      bottom: MediaQuery.paddingOf(context).bottom + 12,
                      child: GestureDetector(
                        onHorizontalDragStart: (_) => _dragX = 0,
                        onHorizontalDragUpdate: (d) => _dragX += d.delta.dx,
                        onHorizontalDragEnd: (_) {
                          if (_dragX.abs() > 45) {
                            unawaited(engine.capture(tab));
                            model.cycleTab(_dragX < 0 ? 1 : -1);
                          }
                        },
                        onVerticalDragStart: (_) => _dragY = 0,
                        onVerticalDragUpdate: (d) => _dragY += d.delta.dy,
                        onVerticalDragEnd: (_) {
                          if (_dragY < -24) _tabs();
                        },
                        child: Glass(
                          child: SizedBox(
                            height: 60,
                            child: overview
                                ? _overviewPill(context)
                                : _navigationPill(context, tab),
                          ),
                        ),
                      ),
                    ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _start(BuildContext context) => LayoutBuilder(
    builder: (c, constraints) => SingleChildScrollView(
      padding: EdgeInsets.fromLTRB(
        34,
        (constraints.maxHeight * .17).clamp(36, 140),
        34,
        120,
      ),
      child: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 380),
          child: Column(
            children: [
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Image.asset(
                    'assets/brand.png',
                    width: 42,
                    height: 42,
                    color: Theme.of(c).colorScheme.onSurface,
                  ),
                  const SizedBox(width: 12),
                  const Text(
                    'Arbiter',
                    style: TextStyle(
                      fontSize: 31,
                      fontWeight: FontWeight.w500,
                      letterSpacing: -.7,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 44),
              GridView.count(
                crossAxisCount: 3,
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                mainAxisSpacing: 16,
                crossAxisSpacing: 14,
                childAspectRatio: .88,
                children: [
                  for (final f in favorites)
                    InkWell(
                      borderRadius: BorderRadius.circular(20),
                      onTap: () => _navigate(f.$2),
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Container(
                            width: 58,
                            height: 58,
                            alignment: Alignment.center,
                            decoration: BoxDecoration(
                              color: Theme.of(c).colorScheme.onSurface
                                  .withValues(alpha: .065),
                              borderRadius: BorderRadius.circular(18),
                            ),
                            child: Text(
                              f.$3,
                              style: const TextStyle(
                                fontSize: 27,
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                          ),
                          const SizedBox(height: 9),
                          Text(
                            f.$1,
                            style: const TextStyle(fontSize: 12),
                            maxLines: 1,
                          ),
                        ],
                      ),
                    ),
                ],
              ),
              if (model.privateMode)
                Padding(
                  padding: const EdgeInsets.only(top: 24),
                  child: Text(
                    'Private browsing · history stays off this device’s saved history',
                    textAlign: TextAlign.center,
                    style: Theme.of(c).textTheme.bodySmall,
                  ),
                ),
            ],
          ),
        ),
      ),
    ),
  );
  Widget _navigationPill(BuildContext c, BrowserTab tab) => Row(
    children: [
      if (tab.url.isNotEmpty)
        IconControl(
          'arrow-left',
          'Back',
          tab.canBack ? () => engine.command<void>('back', id: tab.id) : null,
        ),
      Expanded(
        child: InkWell(
          onTap: _address,
          borderRadius: BorderRadius.circular(22),
          child: Padding(
            padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 8),
            child: Row(
              children: [
                Glyph(
                  tab.url.isEmpty
                      ? 'search'
                      : tab.url.startsWith('https://')
                      ? 'lock'
                      : 'info',
                  size: 17,
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    addressLabel(tab.url),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(fontSize: tab.url.isEmpty ? 13 : 14),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
      Stack(
        alignment: Alignment.bottomCenter,
        children: [
          IconButton(
            tooltip: 'Show tabs',
            onPressed: _tabs,
            icon: Container(
              width: 23,
              height: 24,
              decoration: BoxDecoration(
                border: Border.all(
                  color: Theme.of(c).colorScheme.onSurface,
                  width: 1.6,
                ),
                borderRadius: BorderRadius.circular(6),
              ),
              alignment: Alignment.center,
              child: Text(
                '${model.visibleTabs.length}',
                style: const TextStyle(fontSize: 11),
              ),
            ),
          ),
          if (tab.loading)
            SizedBox(
              width: 22,
              child: LinearProgressIndicator(
                value: tab.progress == 0 ? null : tab.progress,
                minHeight: 2,
              ),
            ),
        ],
      ),
      IconControl('ellipsis', 'More controls', _more),
      const SizedBox(width: 3),
    ],
  );
  Widget _overviewPill(BuildContext c) => Row(
    mainAxisAlignment: MainAxisAlignment.spaceAround,
    children: [
      IconControl(
        'eye-off',
        model.privateMode ? 'Regular tabs' : 'Private tabs',
        () => model.setPrivate(!model.privateMode),
        color: model.privateMode ? Theme.of(c).colorScheme.primary : null,
      ),
      IconButton.filled(
        tooltip: 'New tab',
        onPressed: _newTab,
        icon: const Icon(Icons.add),
      ),
      TextButton(
        onPressed: () => setState(() => overview = false),
        child: const Text('Done'),
      ),
    ],
  );
  Widget _privateShield(BuildContext c) => Center(
    child: Padding(
      padding: const EdgeInsets.all(34),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Glyph('lock', size: 44),
          const SizedBox(height: 24),
          const Text('Private tabs are locked', style: TextStyle(fontSize: 25)),
          const SizedBox(height: 14),
          const Text(
            'Unlock with your biometrics or device passcode.',
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 28),
          FilledButton(onPressed: _unlock, child: const Text('Unlock')),
          const SizedBox(height: 12),
          TextButton(
            onPressed: () => model.setPrivate(false),
            child: const Text('Go to regular tabs'),
          ),
        ],
      ),
    ),
  );
}

class AddressSheet extends StatefulWidget {
  const AddressSheet({super.key, required this.model});
  final BrowserModel model;
  @override
  State<AddressSheet> createState() => _AddressSheetState();
}

class _AddressSheetState extends State<AddressSheet> {
  final input = TextEditingController();
  @override
  void dispose() {
    input.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext c) {
    final query = input.text.trim().toLowerCase();
    final model = widget.model;
    final matches = <(String, String, String)>[];
    final urls = <String>{};
    for (final t in model.visibleTabs) {
      if (t.url.isNotEmpty &&
          '${t.title} ${t.url}'.toLowerCase().contains(query) &&
          urls.add(t.url)) {
        matches.add((t.title, t.url, 'Open tab'));
      }
    }
    if (!model.privateMode) {
      for (final v in model.history) {
        if ('${v.title} ${v.url}'.toLowerCase().contains(query) &&
            urls.add(v.url)) {
          matches.add((v.title, v.url, 'History'));
        }
        if (matches.length >= 7) break;
      }
    }
    for (final f in favorites) {
      if ('${f.$1} ${f.$2}'.toLowerCase().contains(query) && urls.add(f.$2)) {
        matches.add((f.$1, f.$2, ''));
      }
    }
    return Padding(
      padding: EdgeInsets.fromLTRB(
        18,
        0,
        18,
        MediaQuery.viewInsetsOf(c).bottom + MediaQuery.paddingOf(c).bottom + 12,
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          TextField(
            key: const Key('addressInput'),
            controller: input,
            autofocus: true,
            autocorrect: false,
            keyboardType: TextInputType.url,
            textInputAction: TextInputAction.go,
            enableSuggestions: false,
            decoration: InputDecoration(
              hintText: 'Search or enter address',
              prefixIcon: const Icon(Icons.search),
              suffixIcon: input.text.isEmpty
                  ? null
                  : IconButton(
                      onPressed: () {
                        input.clear();
                        setState(() {});
                      },
                      icon: const Icon(Icons.close),
                    ),
            ),
            onChanged: (_) => setState(() {}),
            onSubmitted: (s) {
              if (s.trim().isNotEmpty) Navigator.pop(c, s);
            },
          ),
          const SizedBox(height: 8),
          ConstrainedBox(
            constraints: BoxConstraints(
              maxHeight:
                  (MediaQuery.sizeOf(c).height -
                      MediaQuery.viewInsetsOf(c).bottom) *
                  .45,
            ),
            child: ListView(
              shrinkWrap: true,
              children: [
                if (input.text.trim().isNotEmpty)
                  ListTile(
                    leading: const Glyph('search'),
                    title: Text('Search with ${model.searchEngine}'),
                    subtitle: Text(
                      input.text,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    onTap: () => Navigator.pop(c, input.text),
                  ),
                for (final match in matches.take(7))
                  ListTile(
                    leading: const Glyph('globe'),
                    title: Text(
                      match.$1,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    subtitle: Text(addressLabel(match.$2), maxLines: 1),
                    trailing: match.$3.isEmpty
                        ? null
                        : Text(
                            match.$3,
                            style: Theme.of(c).textTheme.labelSmall,
                          ),
                    onTap: () {
                      if (match.$3 == 'Open tab') {
                        model.selectTab(
                          model.visibleTabs.firstWhere(
                            (t) => t.url == match.$2,
                          ),
                        );
                        Navigator.pop(c);
                      } else {
                        Navigator.pop(c, match.$2);
                      }
                    },
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class TabsOverview extends StatefulWidget {
  const TabsOverview({
    super.key,
    required this.model,
    required this.onSelect,
    required this.onClose,
    required this.onWorkspaces,
  });
  final BrowserModel model;
  final void Function(BrowserTab) onSelect, onClose;
  final VoidCallback onWorkspaces;
  @override
  State<TabsOverview> createState() => _TabsOverviewState();
}

class _TabsOverviewState extends State<TabsOverview> {
  String query = '';
  @override
  Widget build(BuildContext c) {
    final model = widget.model, scheme = Theme.of(c).colorScheme;
    final tabs = model.visibleTabs
        .where(
          (t) =>
              '${t.title} ${t.url}'.toLowerCase().contains(query.toLowerCase()),
        )
        .toList();
    return ColoredBox(
      color: scheme.surface,
      child: CustomScrollView(
        slivers: [
          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(18, 12, 18, 8),
              child: Column(
                children: [
                  Row(
                    children: [
                      TextButton(
                        onPressed: widget.onWorkspaces,
                        child: Row(
                          children: [
                            Text(
                              model.privateMode
                                  ? 'Private · ${model.workspace.name}'
                                  : model.workspace.name,
                              style: TextStyle(
                                fontSize: 25,
                                color: scheme.onSurface,
                              ),
                            ),
                            const SizedBox(width: 8),
                            const Glyph('chevron-down', size: 18),
                          ],
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  TextField(
                    decoration: const InputDecoration(
                      hintText: 'Search tabs',
                      prefixIcon: Icon(Icons.search),
                    ),
                    onChanged: (s) => setState(() => query = s),
                  ),
                  const SizedBox(height: 12),
                ],
              ),
            ),
          ),
          SliverPadding(
            padding: const EdgeInsets.fromLTRB(18, 0, 18, 130),
            sliver: SliverGrid(
              gridDelegate: const SliverGridDelegateWithMaxCrossAxisExtent(
                maxCrossAxisExtent: 250,
                childAspectRatio: .81,
                crossAxisSpacing: 12,
                mainAxisSpacing: 12,
              ),
              delegate: SliverChildBuilderDelegate((c, i) {
                final tab = tabs[i], selected = tab.id == model.activeTab.id;
                return Material(
                  color: selected
                      ? scheme.primary.withValues(alpha: .18)
                      : scheme.onSurface.withValues(alpha: .055),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(22),
                    side: selected
                        ? BorderSide(color: scheme.primary, width: 2)
                        : BorderSide.none,
                  ),
                  clipBehavior: Clip.antiAlias,
                  child: InkWell(
                    onTap: () => widget.onSelect(tab),
                    child: Padding(
                      padding: const EdgeInsets.all(7),
                      child: Column(
                        children: [
                          Expanded(
                            child: Stack(
                              fit: StackFit.expand,
                              children: [
                                ClipRRect(
                                  borderRadius: BorderRadius.circular(15),
                                  child: tab.thumbnail == null
                                      ? ColoredBox(
                                          color: scheme.surface,
                                          child: Center(
                                            child: tab.url.isEmpty
                                                ? Image.asset(
                                                    'assets/brand.png',
                                                    width: 42,
                                                    color: scheme.onSurface,
                                                  )
                                                : const Glyph(
                                                    'globe',
                                                    size: 36,
                                                  ),
                                          ),
                                        )
                                      : Image.memory(
                                          tab.thumbnail!,
                                          fit: BoxFit.cover,
                                          gaplessPlayback: true,
                                        ),
                                ),
                                Positioned(
                                  top: 3,
                                  right: 3,
                                  child: SizedBox(
                                    width: 36,
                                    height: 36,
                                    child: IconButton.filledTonal(
                                      tooltip: 'Close ${tab.title}',
                                      padding: EdgeInsets.zero,
                                      onPressed: () => widget.onClose(tab),
                                      icon: const Icon(Icons.close, size: 17),
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(height: 9),
                          Align(
                            alignment: Alignment.centerLeft,
                            child: Text(
                              tab.title,
                              maxLines: 1,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                          Align(
                            alignment: Alignment.centerLeft,
                            child: Text(
                              tab.url.isEmpty ? '' : addressLabel(tab.url),
                              maxLines: 1,
                              style: TextStyle(
                                fontSize: 11,
                                color: scheme.onSurfaceVariant,
                              ),
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                          const SizedBox(height: 4),
                        ],
                      ),
                    ),
                  ),
                );
              }, childCount: tabs.length),
            ),
          ),
        ],
      ),
    );
  }
}

class WorkspaceSheet extends StatelessWidget {
  const WorkspaceSheet({
    super.key,
    required this.model,
    required this.onAdd,
    required this.onRename,
  });
  final BrowserModel model;
  final VoidCallback onAdd;
  final void Function(Workspace) onRename;
  @override
  Widget build(BuildContext c) => ListenableBuilder(
    listenable: model,
    builder: (c, _) => SafeArea(
      child: SizedBox(
        height: MediaQuery.sizeOf(c).height * .76,
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(22, 0, 12, 10),
              child: Row(
                children: [
                  const Expanded(
                    child: Text('Workspaces', style: TextStyle(fontSize: 25)),
                  ),
                  IconControl('plus', 'New workspace', onAdd),
                ],
              ),
            ),
            Expanded(
              child: ListView.separated(
                padding: const EdgeInsets.fromLTRB(18, 0, 18, 18),
                itemCount: model.workspaces.length,
                separatorBuilder: (_, _) => const SizedBox(height: 12),
                itemBuilder: (c, i) {
                  final w = model.workspaces[i],
                      selected = w.id == model.workspaceId;
                  final tabs = model.tabs
                      .where(
                        (t) =>
                            t.workspaceId == w.id &&
                            t.isPrivate == model.privateMode,
                      )
                      .toList();
                  final scheme = Theme.of(c).colorScheme;
                  return Material(
                    color: selected
                        ? scheme.primary.withValues(alpha: .12)
                        : scheme.onSurface.withValues(alpha: .05),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(22),
                      side: selected
                          ? BorderSide(
                              color: scheme.primary.withValues(alpha: .65),
                            )
                          : BorderSide.none,
                    ),
                    child: InkWell(
                      borderRadius: BorderRadius.circular(22),
                      onTap: () {
                        model.selectWorkspace(w);
                        Navigator.pop(c);
                      },
                      onLongPress: () => onRename(w),
                      child: Padding(
                        padding: const EdgeInsets.all(17),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Row(
                              children: [
                                Container(
                                  width: 13,
                                  height: 13,
                                  decoration: BoxDecoration(
                                    color: Color(w.color),
                                    shape: BoxShape.circle,
                                  ),
                                ),
                                const SizedBox(width: 10),
                                Expanded(
                                  child: Text(
                                    w.name,
                                    style: const TextStyle(
                                      fontSize: 17,
                                      fontWeight: FontWeight.w600,
                                    ),
                                  ),
                                ),
                                if (w.isolated)
                                  const Padding(
                                    padding: EdgeInsets.only(right: 8),
                                    child: Glyph('shield', size: 16),
                                  ),
                                if (selected)
                                  Glyph(
                                    'check',
                                    size: 20,
                                    color: scheme.primary,
                                  ),
                              ],
                            ),
                            const SizedBox(height: 10),
                            for (final tab in tabs.take(3))
                              Padding(
                                padding: const EdgeInsets.symmetric(
                                  vertical: 3,
                                ),
                                child: Row(
                                  children: [
                                    const Glyph('globe', size: 15),
                                    const SizedBox(width: 9),
                                    Expanded(
                                      child: Text(
                                        tab.title,
                                        maxLines: 1,
                                        overflow: TextOverflow.ellipsis,
                                        style: const TextStyle(fontSize: 13),
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                            if (tabs.length > 3)
                              Padding(
                                padding: const EdgeInsets.only(top: 6),
                                child: Text(
                                  '+ ${tabs.length - 3} other tabs',
                                  style: TextStyle(
                                    fontSize: 12,
                                    color: scheme.onSurfaceVariant,
                                  ),
                                ),
                              ),
                            if (tabs.isEmpty)
                              Text(
                                'No tabs yet',
                                style: TextStyle(
                                  color: scheme.onSurfaceVariant,
                                ),
                              ),
                          ],
                        ),
                      ),
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    ),
  );
}

class HistoryScreen extends StatefulWidget {
  const HistoryScreen({super.key, required this.model});
  final BrowserModel model;
  @override
  State<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends State<HistoryScreen> {
  String query = '';
  @override
  Widget build(BuildContext c) {
    final items = widget.model.history
        .where(
          (v) =>
              '${v.title} ${v.url}'.toLowerCase().contains(query.toLowerCase()),
        )
        .toList();
    return Scaffold(
      appBar: AppBar(
        title: const Text('History'),
        actions: [
          IconControl('trash-2', 'Clear history', () async {
            final confirmed = await showDialog<bool>(
              context: c,
              builder: (d) => AlertDialog(
                title: const Text('Clear saved history?'),
                content: Text(
                  query.isEmpty
                      ? 'Remove all saved visits?'
                      : 'Remove only the visits matching this search?',
                ),
                actions: [
                  TextButton(
                    onPressed: () => Navigator.pop(d, false),
                    child: const Text('Cancel'),
                  ),
                  FilledButton(
                    onPressed: () => Navigator.pop(d, true),
                    child: const Text('Clear'),
                  ),
                ],
              ),
            );
            if (confirmed == true) {
              widget.model.history.removeWhere(items.contains);
              widget.model.changed();
              setState(() {});
            }
          }),
        ],
      ),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(18, 0, 18, 14),
            child: TextField(
              decoration: const InputDecoration(
                hintText: 'Search history',
                prefixIcon: Icon(Icons.search),
              ),
              onChanged: (s) => setState(() => query = s),
            ),
          ),
          Expanded(
            child: items.isEmpty
                ? const Center(child: Text('No saved visits'))
                : ListView.builder(
                    itemCount: items.length,
                    itemBuilder: (c, i) {
                      final v = items[i],
                          day = DateUtils.dateOnly(v.time.toLocal());
                      final newDay =
                          i == 0 ||
                          day !=
                              DateUtils.dateOnly(items[i - 1].time.toLocal());
                      final today = DateUtils.dateOnly(DateTime.now());
                      final label = day == today
                          ? 'Today'
                          : day == today.subtract(const Duration(days: 1))
                          ? 'Yesterday'
                          : MaterialLocalizations.of(c).formatFullDate(day);
                      return Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          if (newDay)
                            Padding(
                              padding: const EdgeInsets.fromLTRB(22, 18, 22, 8),
                              child: Text(
                                label,
                                style: Theme.of(c).textTheme.titleSmall,
                              ),
                            ),
                          ListTile(
                            leading: const Glyph('globe'),
                            title: Text(
                              v.title,
                              maxLines: 1,
                              overflow: TextOverflow.ellipsis,
                            ),
                            subtitle: Text(addressLabel(v.url)),
                            onTap: () => Navigator.pop(c, v.url),
                          ),
                        ],
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

class DownloadsScreen extends StatelessWidget {
  const DownloadsScreen({super.key, required this.model});
  final BrowserModel model;
  @override
  Widget build(BuildContext c) => Scaffold(
    appBar: AppBar(title: const Text('Downloads')),
    body: ListenableBuilder(
      listenable: model,
      builder: (c, _) => model.downloads.isEmpty
          ? const Center(child: Text('No downloads yet'))
          : ListView.builder(
              itemCount: model.downloads.length,
              itemBuilder: (c, i) {
                final d = model.downloads[i];
                return ListTile(
                  leading: const Glyph('file'),
                  title: Text(
                    d.name,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                  subtitle: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        d.error ??
                            (d.complete
                                ? 'Completed · ${(d.received / 1048576).toStringAsFixed(1)} MB'
                                : '${(d.received / 1048576).toStringAsFixed(1)} MB downloaded'),
                      ),
                      if (!d.complete && d.error == null)
                        Padding(
                          padding: const EdgeInsets.only(top: 6),
                          child: LinearProgressIndicator(
                            value: d.total > 0 ? d.received / d.total : null,
                          ),
                        ),
                    ],
                  ),
                  trailing: d.complete
                      ? IconControl('share', 'Open or share file', () async {
                          await SharePlus.instance.share(
                            ShareParams(files: [XFile(d.path)]),
                          );
                        })
                      : null,
                );
              },
            ),
    ),
  );
}

class SettingsScreen extends StatelessWidget {
  const SettingsScreen({super.key, required this.model, required this.engine});
  final BrowserModel model;
  final BrowserEngine engine;
  @override
  Widget build(BuildContext c) => Scaffold(
    appBar: AppBar(title: const Text('Settings')),
    body: ListenableBuilder(
      listenable: model,
      builder: (c, _) => ListView(
        padding: const EdgeInsets.fromLTRB(18, 0, 18, 34),
        children: [
          const _SectionLabel('Appearance'),
          SegmentedButton<ThemeMode>(
            segments: const [
              ButtonSegment(value: ThemeMode.system, label: Text('System')),
              ButtonSegment(value: ThemeMode.light, label: Text('Light')),
              ButtonSegment(value: ThemeMode.dark, label: Text('Dark')),
            ],
            selected: {model.themeMode},
            onSelectionChanged: (s) {
              model.themeMode = s.first;
              model.changed();
            },
          ),
          const SizedBox(height: 18),
          _colorRow(
            c,
            'Light accent',
            model.lightAccent,
            (v) => model.lightAccent = v,
          ),
          _colorRow(
            c,
            'Dark accent',
            model.darkAccent,
            (v) => model.darkAccent = v,
          ),
          _colorRow(
            c,
            'Light background',
            model.lightBackground,
            (v) => model.lightBackground = v,
          ),
          _colorRow(
            c,
            'Dark background',
            model.darkBackground,
            (v) => model.darkBackground = v,
          ),
          const _SectionLabel('Browsing'),
          ListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Default browser'),
            subtitle: const Text('Choose Arbiter in your device settings'),
            trailing: const Glyph('external-link'),
            onTap: () => engine.command<void>('requestDefaultBrowser'),
          ),
          ListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Search engine'),
            trailing: DropdownButton<String>(
              value: model.searchEngine,
              items: [
                for (final e in ['DuckDuckGo', 'Google', 'Bing', 'Brave'])
                  DropdownMenuItem(value: e, child: Text(e)),
              ],
              onChanged: (v) {
                if (v != null) {
                  model.searchEngine = v;
                  model.changed();
                }
              },
            ),
          ),
          const _SectionLabel('Private browsing'),
          const Text(
            'Private tabs use separate website storage and are never saved in your tab session or history. They lock whenever the app goes into the background.',
          ),
          const SizedBox(height: 12),
          ListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Lock after inactivity'),
            trailing: DropdownButton<int>(
              value: model.privateLockSeconds,
              items: const [
                DropdownMenuItem(value: 0, child: Text('Off')),
                DropdownMenuItem(value: 60, child: Text('1 minute')),
                DropdownMenuItem(value: 300, child: Text('5 minutes')),
                DropdownMenuItem(value: 900, child: Text('15 minutes')),
              ],
              onChanged: (v) {
                if (v != null) {
                  model.privateLockSeconds = v;
                  model.changed();
                }
              },
            ),
          ),
          const Text(
            'Unlock uses your device’s biometrics or passcode. Files you explicitly download remain on the device.',
          ),
          const _SectionLabel('About'),
          Row(
            children: [
              Image.asset(
                'assets/brand.png',
                width: 44,
                color: Theme.of(c).colorScheme.onSurface,
              ),
              const SizedBox(width: 12),
              const Text('Arbiter', style: TextStyle(fontSize: 26)),
            ],
          ),
          const SizedBox(height: 14),
          const Text(
            'An open-source browser for people who make the web their own. No Arbiter analytics. Everyone is welcome to contribute.',
          ),
          const SizedBox(height: 12),
          const Text(
            'GNU GPL v3 · Lucide icons (ISC). Flutter (BSD), WebKit on iOS, and GeckoView (MPL 2.0) on Android.',
          ),
          ListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('GitHub'),
            trailing: const Glyph('external-link'),
            onTap: () => launchUrl(
              Uri.parse('https://github.com/pouya-eghbali/arbiter'),
              mode: LaunchMode.externalApplication,
            ),
          ),
          ListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Report an issue'),
            trailing: const Glyph('external-link'),
            onTap: () => launchUrl(
              Uri.parse(
                'https://github.com/pouya-eghbali/arbiter/issues/new/choose',
              ),
              mode: LaunchMode.externalApplication,
            ),
          ),
          ListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Open-source licenses'),
            onTap: () =>
                showLicensePage(context: c, applicationName: 'Arbiter'),
          ),
          if (model.storageError != null)
            Text(
              model.storageError!,
              style: TextStyle(color: Theme.of(c).colorScheme.error),
            ),
        ],
      ),
    ),
  );
  Widget _colorRow(
    BuildContext c,
    String name,
    int value,
    void Function(int) set,
  ) => ListTile(
    contentPadding: EdgeInsets.zero,
    title: Text(name),
    trailing: Container(
      width: 34,
      height: 34,
      decoration: BoxDecoration(
        color: Color(value),
        borderRadius: BorderRadius.circular(11),
        border: Border.all(
          color: Theme.of(c).colorScheme.onSurface.withValues(alpha: .15),
        ),
      ),
    ),
    onTap: () async {
      final color = await showDialog<Color>(
        context: c,
        builder: (_) => ColorPickerDialog(initial: Color(value), title: name),
      );
      if (color != null) {
        set(color.toARGB32());
        model.changed();
      }
    },
  );
}

class _SectionLabel extends StatelessWidget {
  const _SectionLabel(this.label);
  final String label;
  @override
  Widget build(BuildContext c) => Padding(
    padding: const EdgeInsets.only(top: 24, bottom: 14),
    child: Text(
      label,
      style: Theme.of(c).textTheme.titleMedium
          ?.copyWith(fontWeight: FontWeight.w600),
    ),
  );
}

class ColorPickerDialog extends StatefulWidget {
  const ColorPickerDialog({
    super.key,
    required this.initial,
    required this.title,
  });
  final Color initial;
  final String title;
  @override
  State<ColorPickerDialog> createState() => _ColorPickerDialogState();
}

class _ColorPickerDialogState extends State<ColorPickerDialog> {
  late HSVColor color = HSVColor.fromColor(widget.initial);
  @override
  Widget build(BuildContext c) => AlertDialog(
    title: Text(widget.title),
    content: SizedBox(
      width: 300,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            height: 72,
            decoration: BoxDecoration(
              color: color.toColor(),
              borderRadius: BorderRadius.circular(18),
            ),
          ),
          const SizedBox(height: 18),
          for (final item in [
            ('Hue', color.hue, 360.0),
            ('Saturation', color.saturation, 1.0),
            ('Brightness', color.value, 1.0),
          ])
            Row(
              children: [
                SizedBox(width: 82, child: Text(item.$1)),
                Expanded(
                  child: Slider(
                    value: item.$2,
                    max: item.$3,
                    onChanged: (v) => setState(() {
                      color = item.$1 == 'Hue'
                          ? color.withHue(v)
                          : item.$1 == 'Saturation'
                          ? color.withSaturation(v)
                          : color.withValue(v);
                    }),
                  ),
                ),
              ],
            ),
        ],
      ),
    ),
    actions: [
      TextButton(
        onPressed: () => Navigator.pop(c),
        child: const Text('Cancel'),
      ),
      FilledButton(
        onPressed: () => Navigator.pop(c, color.toColor()),
        child: const Text('Use color'),
      ),
    ],
  );
}
