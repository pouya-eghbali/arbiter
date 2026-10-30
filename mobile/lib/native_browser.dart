import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter/rendering.dart';

import 'browser_model.dart';

class BrowserEngine {
  BrowserEngine() {
    _receiver = this;
    _channel.setMethodCallHandler(_receive);
  }
  static BrowserEngine? _receiver;
  static const _channel = MethodChannel('com.arbiter.mobile/browser');
  bool userVerificationActive = false;
  void Function(String id, Map<String, dynamic> event)? onEvent;
  Future<dynamic> Function(Map<String, dynamic> request)? onRequest;
  Future<dynamic> _receive(MethodCall call) async {
    final data = Map<String, dynamic>.from(call.arguments as Map? ?? {});
    if (call.method == 'event') {
      onEvent?.call(data['id'] as String? ?? '', data);
      return null;
    }
    return onRequest?.call({...data, 'request': call.method});
  }

  Future<T?> command<T>(
    String action, {
    String? id,
    Map<String, Object?> data = const {},
  }) => _channel.invokeMethod<T>(action, {'id': id, ...data});
  Future<void> load(BrowserTab tab) =>
      command('load', id: tab.id, data: {'url': tab.url});
  Future<void> close(BrowserTab tab) => command('close', id: tab.id);
  Future<void> privateVisible(bool value) =>
      command('privateVisible', data: {'value': value});
  Future<void> capture(BrowserTab tab) async {
    try {
      tab.thumbnail = await command<Uint8List>('snapshot', id: tab.id);
    } on PlatformException {
      /* Pages without a frame use the title preview. */
    }
  }

  void dispose() {
    if (identical(_receiver, this)) {
      _receiver = null;
      _channel.setMethodCallHandler(null);
    }
  }
}

class BrowserSurface extends StatelessWidget {
  const BrowserSurface({
    super.key,
    required this.tab,
    required this.workspace,
    required this.privateContext,
  });
  final BrowserTab tab;
  final Workspace workspace;
  final String privateContext;
  @override
  Widget build(BuildContext context) {
    final params = <String, Object?>{
      'id': tab.id,
      'url': tab.url,
      'private': tab.isPrivate,
      'context': tab.isPrivate
          ? privateContext
          : workspace.isolated
          ? workspace.id
          : null,
    };
    final gestures = <Factory<OneSequenceGestureRecognizer>>{
      Factory(EagerGestureRecognizer.new),
    };
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      return UiKitView(
        viewType: 'com.arbiter.mobile/webview',
        creationParams: params,
        creationParamsCodec: const StandardMessageCodec(),
        gestureRecognizers: gestures,
      );
    }
    if (defaultTargetPlatform == TargetPlatform.android) {
      return PlatformViewLink(
        viewType: 'com.arbiter.mobile/webview',
        surfaceFactory: (context, controller) => AndroidViewSurface(
          controller: controller as AndroidViewController,
          gestureRecognizers: gestures,
          hitTestBehavior: PlatformViewHitTestBehavior.opaque,
        ),
        onCreatePlatformView: (p) =>
            PlatformViewsService.initExpensiveAndroidView(
                id: p.id,
                viewType: 'com.arbiter.mobile/webview',
                layoutDirection: TextDirection.ltr,
                creationParams: params,
                creationParamsCodec: const StandardMessageCodec(),
                onFocus: () => p.onFocusChanged(true),
              )
              ..addOnPlatformViewCreatedListener(p.onPlatformViewCreated)
              ..create(),
      );
    }
    return const Center(
      child: Text('Web browsing is available on Android and iOS.'),
    );
  }
}
