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
    'real native blocker blocks ads, allows normal scripts and honors global/site controls',
    (tester) async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      var ads = 0, content = 0, stops = 0;
      server.listen((request) async {
        request.response.headers.set('Cache-Control', 'no-store');
        if (request.uri.path == '/adserver.js') {
          ads++;
          request.response.headers.contentType = ContentType(
            'application',
            'javascript',
          );
          request.response.write('window.adLoaded=true;');
        } else if (request.uri.path == '/arbiter-ordinary-fixture.js') {
          content++;
          request.response.headers.contentType = ContentType(
            'application',
            'javascript',
          );
          request.response.write('document.title="content-loaded";');
        } else {
          request.response.headers.contentType = ContentType.html;
          request.response.write(
            '<!doctype html><title>Adblock fixture</title><script src="/adserver.js"></script><script src="/arbiter-ordinary-fixture.js"></script><h1>Arbiter fixture</h1>',
          );
        }
        await request.response.close();
      });
      final model = BrowserModel(MemoryStorage()), engine = BrowserEngine();
      var currentUrl = '';
      engine.onEvent = (_, data) {
        if (data['type'] == 'location') {
          currentUrl = data['url'] as String? ?? '';
        }
        if (data['type'] == 'finish' &&
            data['success'] == true &&
            currentUrl.startsWith('http://127.0.0.1:')) {
          stops++;
        }
      };
      engine.onRequest = (_) async => false;
      final origin = 'http://127.0.0.1:${server.port}';
      model.navigate(origin);
      await engine.command<void>(
        'configureAdblock',
        data: {'enabled': true, 'disabledHosts': <String>[]},
      );
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: BrowserSurface(
              tab: model.activeTab,
              workspace: model.workspace,
              privateContext: model.privateContext,
            ),
          ),
        ),
      );
      Future<void> finished(int expected) async {
        final end = DateTime.now().add(const Duration(seconds: 90));
        while (stops < expected && DateTime.now().isBefore(end)) {
          await tester.pump(const Duration(milliseconds: 100));
          await Future<void>.delayed(const Duration(milliseconds: 100));
        }
        expect(stops, expected);
      }

      await finished(1);
      expect(await engine.command<bool>('adblockStatus'), true);
      expect(content, 1);
      expect(ads, 0);
      await engine.command<void>(
        'configureAdblock',
        data: {'enabled': false, 'disabledHosts': <String>[]},
      );
      await engine.command<void>('reload', id: model.activeTab.id);
      await finished(2);
      expect(ads, 1);
      expect(content, 2);
      await engine.command<void>(
        'configureAdblock',
        data: {
          'enabled': true,
          'disabledHosts': ['127.0.0.1'],
        },
      );
      await engine.command<void>('reload', id: model.activeTab.id);
      await finished(3);
      expect(ads, 2);
      expect(content, 3);
      await engine.close(model.activeTab);
      await tester.pumpWidget(const SizedBox());
      engine.dispose();
      model.dispose();
      await server.close(force: true);
    },
  );
}
