import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:arbiter_mobile/browser_model.dart';
import 'package:arbiter_mobile/native_browser.dart';

class MemoryStorage implements SessionStorage {
  @override
  Future<String?> read() async => null;
  @override
  Future<void> write(String value) async {}
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  testWidgets(
    'native browsing, history, storage isolation, capture, and origin-bound fill',
    (tester) async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final cookies = <String, String>{};
      server.listen((request) async {
        final path = request.uri.path;
        cookies[path] = request.headers.value(HttpHeaders.cookieHeader) ?? '';
        if (path == '/regular') {
          request.response.cookies.add(
            Cookie('arbiter_fixture', 'regular')..path = '/',
          );
        }
        request.response.headers.contentType = ContentType.html;
        request.response.write(
          '''<!doctype html><html><head><title>${path.substring(1)}</title>
        <meta name="viewport" content="width=device-width,initial-scale=1"></head>
        <body style="background:#fff;color:#111;font:24px sans-serif;padding:24px">
        <h1>Arbiter native browser test</h1><form>
        <input name="username" autocomplete="username" placeholder="Username">
        <input name="password" type="password" autocomplete="current-password" placeholder="Password">
        </form><script>document.addEventListener('input',()=>{
        if(document.querySelector('[name=password]').value) document.title='filled:'+document.querySelector('[name=username]').value;
        });</script></body></html>''',
        );
        await request.response.close();
      });
      final origin = 'http://127.0.0.1:${server.port}';
      final model = BrowserModel(MemoryStorage()), engine = BrowserEngine();
      final titles = <String, String>{}, finished = <String, bool>{};
      engine.onEvent = (id, data) {
        if (data['type'] == 'title') titles[id] = data['title'] as String;
        if (data['type'] == 'finish') finished[id] = data['success'] == true;
        model.engineEvent(id, data);
      };
      engine.onRequest = (_) async => false;
      model.navigate('$origin/regular');
      await tester.pumpWidget(
        MaterialApp(
          home: ListenableBuilder(
            listenable: model,
            builder: (_, _) => Scaffold(
              body: SafeArea(
                child: BrowserSurface(
                  key: ValueKey(model.activeTab.id),
                  tab: model.activeTab,
                  workspace: model.workspace,
                  privateContext: model.privateContextFor(model.workspace),
                ),
              ),
            ),
          ),
        ),
      );
      Future<void> until(bool Function() condition, String reason) async {
        final end = DateTime.now().add(const Duration(seconds: 35));
        while (!condition() && DateTime.now().isBefore(end)) {
          await tester.pump(const Duration(milliseconds: 100));
        }
        expect(condition(), true, reason: reason);
      }

      final regular = model.activeTab;
      await until(
        () => finished[regular.id] == true && titles[regular.id] == 'regular',
        'regular page loads in the native engine',
      );
      expect(model.history.single.url, '$origin/regular');
      model.navigate('$origin/second');
      await engine.load(regular);
      await until(
        () => titles[regular.id] == 'second' && regular.canBack,
        'native back history is populated',
      );
      expect(cookies['/second'], contains('arbiter_fixture=regular'));
      await engine.command<void>('back', id: regular.id);
      await until(
        () => titles[regular.id] == 'regular' && regular.canForward,
        'native back navigation works',
      );
      await engine.command<void>('forward', id: regular.id);
      await until(
        () => titles[regular.id] == 'second',
        'native forward navigation works',
      );
      final denied = await engine.command<bool>(
        'fill',
        id: regular.id,
        data: {
          'origin': 'https://unrelated.example',
          'username': 'fixture-user',
          'password': 'fixture-only',
        },
      );
      expect(
        denied,
        false,
        reason: 'a mismatched origin must never receive a password',
      );
      bool filled = false;
      for (var i = 0; i < 20 && !filled; i++) {
        filled =
            await engine.command<bool>(
              'fill',
              id: regular.id,
              data: {
                'origin': origin,
                'username': 'fixture-user',
                'password': 'fixture-only',
              },
            ) ??
            false;
        await tester.pump(const Duration(milliseconds: 150));
      }
      expect(
        filled,
        true,
        reason: 'the isolated native bridge fills only the selected origin',
      );
      await until(
        () => titles[regular.id] == 'filled:fixture-user',
        'filled fields dispatch input events to the page',
      );
      await engine.capture(regular);
      expect(
        regular.thumbnail,
        isNotEmpty,
        reason: 'native page capture produces a tab preview',
      );
      model.setPrivate(true);
      model.navigate('$origin/private');
      final private = model.activeTab;
      await until(
        () => titles[private.id] == 'private',
        'a private native session can browse',
      );
      expect(cookies['/private'], isNot(contains('arbiter_fixture')));
      expect(jsonEncode(model.snapshot()), isNot(contains('$origin/private')));
      model.setPrivate(false);
      model.addWorkspace('Isolated', isolated: true);
      model.navigate('$origin/isolated');
      final isolated = model.activeTab;
      await until(
        () => titles[isolated.id] == 'isolated',
        'an isolated workspace can browse',
      );
      expect(cookies['/isolated'], isNot(contains('arbiter_fixture')));
      for (final tab in model.tabs.toList()) {
        await engine.close(tab);
      }
      await tester.pumpWidget(const SizedBox());
      engine.dispose();
      model.dispose();
      await server.close(force: true);
    },
  );
}
