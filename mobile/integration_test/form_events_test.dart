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
    'isolated form capture accepts requestSubmit, rejects synthetic submits and obeys save preferences',
    (tester) async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      server.listen((request) async {
        await request.drain<void>();
        request.response.headers.contentType = ContentType.html;
        request.response.write(
          request.uri.path == '/done'
              ? '<title>fixture-done</title><h1>Done</h1>'
              : '''<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><title>fixture-form</title>
      <form action="/done" method="post"><input name="username" autocomplete="username" value="fixture-user" style="position:absolute;left:20px;top:30px;width:240px;height:40px">
      <input name="password" type="password" value="fixture-password" style="position:absolute;left:20px;top:90px;width:240px;height:40px">
      <button type="submit" style="position:absolute;left:20px;top:180px;width:240px;height:50px">Sign in</button>
      <button type="button" style="position:absolute;left:20px;top:250px;width:240px;height:50px" onclick="const fake=new Event('submit',{bubbles:true,cancelable:true});fake.preventDefault();document.querySelector('form').dispatchEvent(fake);">Synthetic submit</button></form><script>
      setTimeout(()=>{const fake=new Event('submit',{bubbles:true,cancelable:true});fake.preventDefault();document.querySelector('form').dispatchEvent(fake);document.title='fixture-synthetic'},500);
      setTimeout(()=>document.querySelector('form').requestSubmit(),3000);
      </script>''',
        );
        await request.response.close();
      });
      final model = BrowserModel(MemoryStorage()), engine = BrowserEngine();
      final captured = <Map<String, dynamic>>[];
      var title = '';
      final seenTitles = <String>{};
      final events = <String>[];
      engine.onEvent = (_, data) {
        events.add(data['type'] as String);
        if (data['type'] == 'title') {
          title = data['title'] as String;
          seenTitles.add(title);
        }
        if (data['type'] == 'loginSubmitted') captured.add(data);
      };
      engine.onRequest = (_) async => false;
      final origin = 'http://127.0.0.1:${server.port}';
      model.navigate('$origin/form');
      await engine.command<void>(
        'configureCredentialPreferences',
        data: {'savePasswords': true, 'suggestPasswords': true},
      );
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SafeArea(
              child: BrowserSurface(
                tab: model.activeTab,
                workspace: model.workspace,
                privateContext: model.privateContext,
              ),
            ),
          ),
        ),
      );
      Future<void> until(bool Function() condition) async {
        final end = DateTime.now().add(const Duration(seconds: 30));
        while (!condition() && DateTime.now().isBefore(end)) {
          await tester.pump(const Duration(milliseconds: 100));
          await Future<void>.delayed(const Duration(milliseconds: 100));
        }
        expect(condition(), true, reason: events.join(','));
      }

      await until(() => title == 'fixture-form');
      await until(() => seenTitles.contains('fixture-synthetic'));
      expect(captured, isEmpty);
      await until(() => title == 'fixture-done');
      await until(() => captured.isNotEmpty);
      expect(captured.length, 1, reason: events.join(","));
      expect(captured.single['origin'], origin);
      expect(captured.single['username'], 'fixture-user');
      expect(captured.single['password'], 'fixture-password');
      captured.clear();
      await engine.command<void>(
        'configureCredentialPreferences',
        data: {'savePasswords': false, 'suggestPasswords': false},
      );
      title = '';
      await engine.load(model.activeTab);
      await until(() => title == 'fixture-form');
      await until(() => title == 'fixture-done');
      expect(captured, isEmpty);
      await engine.close(model.activeTab);
      await tester.pumpWidget(const SizedBox());
      engine.dispose();
      model.dispose();
      await server.close(force: true);
    },
  );
}
