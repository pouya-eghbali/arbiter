import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:arbiter_mobile/browser_model.dart';

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
  test(
    'private tabs, visits, and downloads never enter the saved session',
    () async {
      final storage = MemoryStorage(), model = BrowserModel(MemoryStorage());
      model.navigate('https://public.example/');
      model.engineEvent(model.activeTab.id, {
        'type': 'finish',
        'success': true,
      });
      model.setPrivate(true);
      model.navigate('https://private.example/secret');
      model.engineEvent(model.activeTab.id, {
        'type': 'finish',
        'success': true,
      });
      model.downloadEvent({
        'downloadId': 'private-file',
        'name': 'secret.pdf',
        'private': true,
      });
      final encoded = jsonEncode(model.snapshot());
      expect(encoded, contains('public.example'));
      expect(encoded, isNot(contains('private.example')));
      expect(encoded, isNot(contains('secret.pdf')));
      expect(model.history.length, 1);
      storage.value = encoded;
      final restored = await BrowserModel.restore(storage);
      expect(restored.privateMode, false);
      expect(restored.tabs.every((t) => !t.isPrivate), true);
      model.dispose();
      restored.dispose();
    },
  );
  test('closing the last private tab returns to regular browsing', () {
    final model = BrowserModel(MemoryStorage());
    model.setPrivate(true);
    model.closeTab(model.activeTab);
    expect(model.privateMode, false);
    expect(model.tabs.any((t) => t.isPrivate), false);
    model.dispose();
  });
  test('cycling tabs stays in the current workspace and browsing mode', () {
    final model = BrowserModel(MemoryStorage());
    final personal = model.activeTab;
    final second = model.addTab();
    model.selectTab(personal);
    model.cycleTab(1);
    expect(model.activeTab.id, second.id);
    model.addWorkspace('Work', isolated: true);
    final work = model.activeTab;
    model.cycleTab(1);
    expect(model.activeTab.id, work.id);
    model.setPrivate(true);
    final private = model.activeTab;
    model.cycleTab(-1);
    expect(model.activeTab.id, private.id);
    model.dispose();
  });
  test('unknown schemes cannot become executable browser navigations', () {
    for (final value in [
      'javascript:alert(1)',
      'file:///etc/passwd',
      'data:text/html,hello',
    ]) {
      expect(addressUri(value, 'DuckDuckGo').scheme, 'https');
      expect(isWebAddress(value), false);
    }
    expect(isWebAddress('https://user:password@example.com'), false);
    expect(addressUri('example.com', 'DuckDuckGo').host, 'example.com');
    expect(addressUri('two words', 'Bing').host, 'www.bing.com');
  });
  test(
    'a malformed saved session recovers a usable workspace and tab',
    () async {
      final storage = MemoryStorage()
        ..value = '{"version":1,"workspaces":[{"id":5}]}';
      final model = await BrowserModel.restore(storage);
      expect(model.workspaces, isNotEmpty);
      expect(model.activeTab.workspaceId, model.workspaceId);
      model.dispose();
    },
  );
}
