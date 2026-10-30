import 'package:flutter/widgets.dart';
import 'package:flutter/services.dart';

import 'credential_vault.dart';

/// Runs in a headless engine. Native scheduling is best-effort and never creates a vault.
Future<void> runBackgroundSync({
  Future<CredentialVault?> Function()? openVault,
}) async {
  WidgetsFlutterBinding.ensureInitialized();
  CredentialVault? vault;
  var success = false, cancelled = false;
  const channel = MethodChannel('com.arbiter.mobile/background');
  channel.setMethodCallHandler((call) async {
    if (call.method == 'cancel') {
      cancelled = true;
      vault?.dispose();
    }
  });
  try {
    vault = await (openVault?.call() ?? CredentialVault.openExistingForSync());
    if (cancelled) {
      vault?.dispose();
      return;
    }
    if (vault?.peer != null) await vault!.sync();
    success = true;
  } catch (_) {
    // An OS-locked keychain or an offline desktop is expected; retry next time.
  } finally {
    vault?.dispose();
    await const MethodChannel('com.arbiter.mobile/background')
        .invokeMethod<void>('complete', {'success': success});
  }
}
