import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import 'browser_model.dart';
import 'browser_shell.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
  LicenseRegistry.addLicense(() async* {
    for (final entry in {
      'Arbiter': 'Arbiter.txt',
      'Lucide': 'Lucide.txt',
      'GeckoView': 'GeckoView.txt',
    }.entries) {
      yield LicenseEntryWithLineBreaks([
        entry.key,
      ], await rootBundle.loadString('assets/licenses/${entry.value}'));
    }
  });
  final model = await BrowserModel.restore(DeviceSessionStorage());
  runApp(ArbiterApp(model: model));
}

class ArbiterApp extends StatelessWidget {
  const ArbiterApp({super.key, required this.model});
  final BrowserModel model;
  ThemeData theme(Brightness brightness) {
    final dark = brightness == Brightness.dark;
    final background = Color(
      dark ? model.darkBackground : model.lightBackground,
    );
    final accent = Color(dark ? model.darkAccent : model.lightAccent);
    final scheme = ColorScheme.fromSeed(
      seedColor: accent,
      brightness: brightness,
      primary: accent,
      onPrimary: accent.computeLuminance() > .179 ? Colors.black : Colors.white,
      surface: background,
    );
    return ThemeData(
      useMaterial3: true,
      colorScheme: scheme,
      scaffoldBackgroundColor: background,
      splashFactory: InkSparkle.splashFactory,
      appBarTheme: AppBarTheme(
        backgroundColor: background,
        elevation: 0,
        scrolledUnderElevation: 0,
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: scheme.onSurface.withValues(alpha: .065),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(22),
          borderSide: BorderSide.none,
        ),
        contentPadding: const EdgeInsets.symmetric(
          horizontal: 18,
          vertical: 16,
        ),
      ),
      bottomSheetTheme: BottomSheetThemeData(
        backgroundColor: background,
        showDragHandle: true,
        modalBarrierColor: Colors.black45,
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(top: Radius.circular(30)),
        ),
      ),
      dialogTheme: DialogThemeData(
        backgroundColor: background,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(26)),
      ),
    );
  }

  @override
  Widget build(BuildContext context) => ListenableBuilder(
    listenable: model,
    builder: (context, _) => MaterialApp(
      title: 'Arbiter',
      debugShowCheckedModeBanner: false,
      localizationsDelegates: GlobalMaterialLocalizations.delegates,
      supportedLocales: const [
        Locale('en'),
        Locale('de'),
        Locale('fr'),
        Locale('it'),
        Locale('es'),
        Locale('pt'),
        Locale('ja'),
        Locale('ko'),
        Locale('zh'),
        Locale('ar'),
      ],
      theme: theme(Brightness.light),
      darkTheme: theme(Brightness.dark),
      themeMode: model.themeMode,
      home: BrowserShell(model: model),
    ),
  );
}
