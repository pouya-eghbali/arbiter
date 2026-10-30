import 'dart:async';
import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

String newId() {
  final random = Random.secure();
  final bytes = List.generate(16, (_) => random.nextInt(256));
  bytes[6] = (bytes[6] & 15) | 64;
  bytes[8] = (bytes[8] & 63) | 128;
  final hex = bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join();
  return '${hex.substring(0, 8)}-${hex.substring(8, 12)}-${hex.substring(12, 16)}-${hex.substring(16, 20)}-${hex.substring(20)}';
}

Uri addressUri(String input, String searchEngine) {
  final text = input.trim();
  if (text.isEmpty) throw const FormatException('Enter an address or search.');
  final local = RegExp(r'^(localhost|127\.\d+\.\d+\.\d+|\[::1\])(:\d+)?(/|$)')
      .hasMatch(text);
  final candidate = Uri.tryParse(
    text.contains('://') ? text : '${local ? 'http' : 'https'}://$text',
  );
  if (!text.contains(RegExp(r'\s')) &&
      candidate != null &&
      (candidate.scheme == 'https' || candidate.scheme == 'http') &&
      candidate.host.isNotEmpty &&
      (text.contains('://') || local || candidate.host.contains('.'))) {
    if (candidate.userInfo.isNotEmpty) {
      throw const FormatException(
        'Enter an address without a username or password.',
      );
    }
    return candidate;
  }
  return switch (searchEngine) {
    'Google' => Uri.https('www.google.com', '/search', {'q': text}),
    'Bing' => Uri.https('www.bing.com', '/search', {'q': text}),
    'Brave' => Uri.https('search.brave.com', '/search', {'q': text}),
    _ => Uri.https('duckduckgo.com', '/', {'q': text}),
  };
}

bool isWebAddress(String value) {
  final uri = Uri.tryParse(value);
  return uri != null &&
      const ['https', 'http'].contains(uri.scheme) &&
      uri.host.isNotEmpty &&
      uri.userInfo.isEmpty;
}

String addressLabel(String value) {
  final uri = Uri.tryParse(value);
  if (uri == null || uri.host.isEmpty) return 'Search or enter address';
  return uri.host.replaceFirst(RegExp(r'^www\.'), '');
}

abstract interface class SessionStorage {
  Future<String?> read();
  Future<void> write(String value);
}

class DeviceSessionStorage implements SessionStorage {
  final _preferences = SharedPreferencesAsync();
  @override
  Future<String?> read() => _preferences.getString('arbiter.browser.v1');
  @override
  Future<void> write(String value) =>
      _preferences.setString('arbiter.browser.v1', value);
}

class Workspace {
  Workspace({
    required this.id,
    required this.name,
    this.color = 0xffab75ff,
    this.isolated = false,
  });
  final String id;
  String name;
  int color;
  final bool isolated;
  Map<String, Object> toJson() => {
    'id': id,
    'name': name,
    'color': color,
    'isolated': isolated,
  };
}

class BrowserTab {
  BrowserTab({
    required this.id,
    required this.workspaceId,
    this.isPrivate = false,
    this.url = '',
    this.title = 'New tab',
  });
  final String id;
  final String workspaceId;
  final bool isPrivate;
  String url;
  String title;
  double progress = 0;
  bool loading = false, canBack = false, canForward = false;
  String? error;
  DateTime lastUsed = DateTime.now();
  bool sleeping = false, playingAudio = false, editedForm = false;
  Uint8List? thumbnail;
  Map<String, Object> toJson() => {
    'id': id,
    'workspace': workspaceId,
    'url': url,
    'title': title,
  };
}

class Visit {
  Visit({required this.url, required this.title, required this.time});
  final String url, title;
  final DateTime time;
  Map<String, Object> toJson() => {
    'url': url,
    'title': title,
    'time': time.toIso8601String(),
  };
}

class DownloadRecord {
  DownloadRecord({
    required this.id,
    required this.name,
    this.path = '',
    this.received = 0,
    this.total = -1,
    this.complete = false,
    this.error,
    this.isPrivate = false,
  });
  final String id;
  String name, path;
  int received, total;
  bool complete;
  final bool isPrivate;
  String? error;
  Map<String, Object?> toJson() => {
    'id': id,
    'name': name,
    'path': path,
    'received': received,
    'total': total,
    'complete': complete,
    'error': error,
  };
}

class BrowserModel extends ChangeNotifier {
  BrowserModel(this.storage) {
    workspaces.add(
      Workspace(id: '00000000-0000-4000-8000-000000000001', name: 'Personal'),
    );
    workspaceId = workspaces.first.id;
    _createTab(false);
  }
  final SessionStorage storage;
  final List<Workspace> workspaces = [];
  final List<BrowserTab> tabs = [];
  final List<Visit> history = [];
  final List<DownloadRecord> downloads = [];
  late String workspaceId;
  String? _normalId, _privateId;
  String privateContext = newId();
  final Map<String, String> _privateContexts = {};
  String privateContextFor(Workspace workspace) => workspace.isolated
      ? _privateContexts.putIfAbsent(workspace.id, newId)
      : privateContext;
  bool privateMode = false, privateLocked = false;
  ThemeMode themeMode = ThemeMode.system;
  String searchEngine = 'DuckDuckGo';
  int lightAccent = 0xff7947c7, darkAccent = 0xffad79ff;
  int lightBackground = 0xfff4f3f7, darkBackground = 0xff101720;
  int privateLockSeconds = 60;
  int tabSleepSeconds = 600, passwordUnlockSeconds = 300;
  bool adblockAvailable = true;
  bool adblockEnabled = true,
      savePasswords = true,
      suggestPasswords = true,
      backgroundSync = true;
  bool onboardingDone = false;
  String language = 'system';
  final Set<String> adblockDisabledHosts = {};
  bool blocksAds(String url) =>
      adblockEnabled &&
      adblockAvailable &&
      !adblockDisabledHosts.contains(Uri.tryParse(url)?.host.toLowerCase());
  void toggleSiteBlocking(String url) {
    final host = Uri.tryParse(url)?.host.toLowerCase();
    if (host == null || host.isEmpty) return;
    if (!adblockDisabledHosts.add(host)) adblockDisabledHosts.remove(host);
    changed();
  }

  List<BrowserTab> sleepCandidates(DateTime now) => tabSleepSeconds <= 0
      ? []
      : tabs
            .where(
              (tab) =>
                  tab != activeTab &&
                  !tab.sleeping &&
                  !tab.loading &&
                  !tab.playingAudio &&
                  !tab.editedForm &&
                  tab.url.isNotEmpty &&
                  now.difference(tab.lastUsed).inSeconds >= tabSleepSeconds,
            )
            .toList();
  String? storageError;
  Timer? _saveTimer;
  Future<void> _writes = Future.value();

  static Future<BrowserModel> restore(SessionStorage storage) async {
    final model = BrowserModel(storage);
    try {
      final raw = await storage.read();
      if (raw == null) return model;
      if (raw.length > 4 * 1024 * 1024) {
        throw const FormatException('Session is too large');
      }
      final data = jsonDecode(raw) as Map<String, dynamic>;
      final groups = (data['workspaces'] as List? ?? [])
          .take(100)
          .whereType<Map>()
          .map(
            (m) => Workspace(
              id: m['id'] as String,
              name: m['name'] as String,
              color: m['color'] as int? ?? 0xffab75ff,
              isolated: m['isolated'] == true,
            ),
          )
          .toList();
      if (groups.isNotEmpty) {
        model.workspaces
          ..clear()
          ..addAll(groups);
        model.workspaceId = groups.any((w) => w.id == data['workspace'])
            ? data['workspace'] as String
            : groups.first.id;
        model.tabs.clear();
        for (final m
            in (data['tabs'] as List? ?? []).take(200).whereType<Map>()) {
          if (!groups.any((w) => w.id == m['workspace'])) continue;
          final url = m['url'] as String? ?? '';
          if (url.isNotEmpty && !isWebAddress(url)) continue;
          model.tabs.add(
            BrowserTab(
              id: m['id'] as String,
              workspaceId: m['workspace'] as String,
              url: url,
              title: m['title'] as String? ?? 'New tab',
            ),
          );
        }
        model._normalId = data['active'] as String?;
        model._ensureTab();
      }
      final settings = data['settings'] as Map? ?? {};
      model.themeMode = ThemeMode.values.firstWhere(
        (v) => v.name == settings['theme'],
        orElse: () => ThemeMode.system,
      );
      model.searchEngine =
          const [
            'DuckDuckGo',
            'Google',
            'Bing',
            'Brave',
          ].contains(settings['search'])
          ? settings['search'] as String
          : 'DuckDuckGo';
      model.lightAccent = settings['lightAccent'] as int? ?? model.lightAccent;
      model.darkAccent = settings['darkAccent'] as int? ?? model.darkAccent;
      model.lightBackground =
          settings['lightBackground'] as int? ?? model.lightBackground;
      model.darkBackground =
          settings['darkBackground'] as int? ?? model.darkBackground;
      model.privateLockSeconds =
          const [0, 60, 300, 900].contains(settings['privateLockSeconds'])
          ? settings['privateLockSeconds'] as int
          : 60;
      model.tabSleepSeconds = (settings['tabSleepSeconds'] as int? ?? 600)
          .clamp(0, 86400);
      model.passwordUnlockSeconds =
          (settings['passwordUnlockSeconds'] as int? ?? 300).clamp(0, 3600);
      model.adblockEnabled = settings['adblockEnabled'] != false;
      model.savePasswords = settings['savePasswords'] != false;
      model.suggestPasswords = settings['suggestPasswords'] != false;
      model.backgroundSync = settings['backgroundSync'] != false;
      model.onboardingDone = settings['onboardingDone'] == true;
      model.language =
          const [
            'system',
            'en',
            'de',
            'fr',
            'it',
            'es',
            'pt',
            'ja',
            'zh',
          ].contains(settings['language'])
          ? settings['language'] as String
          : 'system';
      if (!const [0, 300, 600, 1800].contains(model.tabSleepSeconds)) {
        model.tabSleepSeconds = 600;
      }
      if (!const [0, 60, 300, 900].contains(model.passwordUnlockSeconds)) {
        model.passwordUnlockSeconds = 300;
      }
      model.adblockDisabledHosts.addAll(
        (settings['adblockDisabledHosts'] as List? ?? [])
            .whereType<String>()
            .take(1000),
      );
      for (final m
          in (data['history'] as List? ?? []).take(2000).whereType<Map>()) {
        if (isWebAddress(m['url'] as String? ?? '')) {
          model.history.add(
            Visit(
              url: m['url'] as String,
              title: m['title'] as String,
              time: DateTime.parse(m['time'] as String),
            ),
          );
        }
      }
      for (final m
          in (data['downloads'] as List? ?? []).take(200).whereType<Map>()) {
        model.downloads.add(
          DownloadRecord(
            id: m['id'] as String,
            name: m['name'] as String,
            path: m['path'] as String? ?? '',
            received: m['received'] as int? ?? 0,
            total: m['total'] as int? ?? -1,
            complete: m['complete'] == true,
            error: m['complete'] == true
                ? m['error'] as String?
                : 'Download interrupted.',
          ),
        );
      }
    } catch (_) {
      model.storageError = 'Could not restore the previous browsing session.';
    }
    return model;
  }

  Workspace get workspace => workspaces.firstWhere((w) => w.id == workspaceId);
  List<BrowserTab> get visibleTabs => tabs
      .where((t) => t.workspaceId == workspaceId && t.isPrivate == privateMode)
      .toList();
  BrowserTab get activeTab {
    _ensureTab();
    final id = privateMode ? _privateId : _normalId;
    return visibleTabs.firstWhere(
      (t) => t.id == id,
      orElse: () => visibleTabs.first,
    );
  }

  void _ensureTab() {
    if (visibleTabs.isEmpty) _createTab(privateMode);
    final id = privateMode ? _privateId : _normalId;
    if (!visibleTabs.any((t) => t.id == id)) {
      if (privateMode) {
        _privateId = visibleTabs.first.id;
      } else {
        _normalId = visibleTabs.first.id;
      }
    }
  }

  BrowserTab _createTab(bool private, {String url = ''}) {
    final tab = BrowserTab(
      id: newId(),
      workspaceId: workspaceId,
      isPrivate: private,
      url: url,
    );
    tabs.add(tab);
    if (private) {
      _privateId = tab.id;
    } else {
      _normalId = tab.id;
    }
    return tab;
  }

  BrowserTab addTab({String url = '', bool? private}) {
    privateMode = private ?? privateMode;
    final tab = _createTab(privateMode, url: url);
    changed();
    return tab;
  }

  void selectTab(BrowserTab tab) {
    activeTab.lastUsed = DateTime.now();
    tab.lastUsed = DateTime.now();
    tab.sleeping = false;
    workspaceId = tab.workspaceId;
    privateMode = tab.isPrivate;
    if (privateMode) {
      _privateId = tab.id;
    } else {
      _normalId = tab.id;
    }
    changed();
  }

  void cycleTab(int delta) {
    final list = visibleTabs;
    if (list.length < 2) return;
    selectTab(list[(list.indexOf(activeTab) + delta) % list.length]);
  }

  void closeTab(BrowserTab tab) {
    tabs.remove(tab);
    if (!tabs.any((t) => t.isPrivate)) {
      privateContext = newId();
      _privateContexts.clear();
      privateLocked = false;
      if (tab.isPrivate) privateMode = false;
      downloads.removeWhere((d) => d.isPrivate);
    }
    _ensureTab();
    changed();
  }

  void setPrivate(bool value) {
    privateMode = value;
    _ensureTab();
    changed();
  }

  void selectWorkspace(Workspace value) {
    workspaceId = value.id;
    _ensureTab();
    changed();
  }

  Workspace addWorkspace(String name, {bool isolated = false}) {
    final w = Workspace(
      id: newId(),
      name: name.trim(),
      isolated: isolated,
      color: const [
        0xffad79ff,
        0xff69acff,
        0xff6bc894,
        0xfff197be,
      ][workspaces.length % 4],
    );
    workspaces.add(w);
    selectWorkspace(w);
    return w;
  }

  void renameWorkspace(Workspace value, String name) {
    value.name = name.trim();
    changed();
  }

  void navigate(String input) {
    final uri = addressUri(input, searchEngine);
    final tab = activeTab;
    tab.lastUsed = DateTime.now();
    tab.sleeping = false;
    tab.editedForm = false;
    tab.url = uri.toString();
    tab.error = null;
    tab.loading = true;
    tab.title = uri.host;
    changed();
  }

  void engineEvent(String id, Map<String, dynamic> data) {
    final tab = tabs.where((t) => t.id == id).firstOrNull;
    if (tab == null) return;
    switch (data['type']) {
      case 'activityState':
        tab.playingAudio = data['audio'] == true;
        tab.editedForm = data['dirty'] == true;
      case 'location':
        if (isWebAddress(data['url'] as String? ?? '')) {
          tab.url = data['url'] as String;
        }
        tab.canBack = data['back'] as bool? ?? tab.canBack;
        tab.canForward = data['forward'] as bool? ?? tab.canForward;
      case 'title':
        final title = (data['title'] as String? ?? '').trim();
        if (title.isNotEmpty) tab.title = title;
      case 'progress':
        tab.progress = (data['progress'] as num).toDouble().clamp(0, 1);
        tab.loading = tab.progress < 1;
        if (tab.loading) tab.error = null;
      case 'finish':
        tab.loading = false;
        tab.progress = 1;
        if (data['success'] == true &&
            !tab.isPrivate &&
            isWebAddress(tab.url)) {
          final now = DateTime.now();
          if (history.isEmpty ||
              history.first.url != tab.url ||
              now.difference(history.first.time).inSeconds > 15) {
            history.insert(0, Visit(url: tab.url, title: tab.title, time: now));
            if (history.length > 2000) history.removeLast();
          }
        }
      case 'error':
        tab.error =
            data['message'] as String? ?? 'This page could not be loaded.';
        tab.loading = false;
    }
    changed();
  }

  void downloadEvent(Map<String, dynamic> data) {
    final id = data['downloadId'] as String;
    final d =
        downloads.where((d) => d.id == id).firstOrNull ??
        DownloadRecord(
          id: id,
          name: data['name'] as String? ?? 'Download',
          isPrivate: data['private'] == true,
        );
    if (!downloads.contains(d)) downloads.insert(0, d);
    d.path = data['path'] as String? ?? d.path;
    d.received = (data['received'] as num?)?.toInt() ?? d.received;
    d.total = (data['total'] as num?)?.toInt() ?? d.total;
    d.complete = data['complete'] == true;
    d.error = data['error'] as String?;
    changed();
  }

  Map<String, Object?> snapshot() => {
    'version': 1,
    'workspace': workspaceId,
    'active': _normalId,
    'workspaces': workspaces.map((w) => w.toJson()).toList(),
    'tabs': tabs.where((t) => !t.isPrivate).map((t) => t.toJson()).toList(),
    'history': history.map((v) => v.toJson()).toList(),
    'downloads': downloads
        .where((d) => !d.isPrivate)
        .map((d) => d.toJson())
        .toList(),
    'settings': {
      'theme': themeMode.name,
      'search': searchEngine,
      'lightAccent': lightAccent,
      'darkAccent': darkAccent,
      'lightBackground': lightBackground,
      'darkBackground': darkBackground,
      'privateLockSeconds': privateLockSeconds,
      'tabSleepSeconds': tabSleepSeconds,
      'passwordUnlockSeconds': passwordUnlockSeconds,
      'adblockEnabled': adblockEnabled,
      'adblockDisabledHosts': adblockDisabledHosts.toList(),
      'savePasswords': savePasswords,
      'suggestPasswords': suggestPasswords,
      'backgroundSync': backgroundSync,
      'onboardingDone': onboardingDone,
      'language': language,
    },
  };
  void changed() {
    notifyListeners();
    _saveTimer?.cancel();
    _saveTimer = Timer(const Duration(milliseconds: 300), flush);
  }

  Future<void> flush() {
    _saveTimer?.cancel();
    final data = jsonEncode(snapshot());
    _writes = _writes
        .catchError((Object _) {})
        .then((_) => storage.write(data))
        .catchError((Object _) {
          storageError = 'Could not save browser settings.';
          notifyListeners();
        });
    return _writes;
  }

  @override
  void dispose() {
    _saveTimer?.cancel();
    super.dispose();
  }
}
