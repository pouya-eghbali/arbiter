import 'dart:convert';
import 'dart:ui' show PlatformDispatcher;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

class MobileStrings {
  MobileStrings(this.locale);
  final Locale locale;
  static String language = 'system';
  static final Map<String, Map<String, String>> catalogs = {};
  static const codes = ['en', 'de', 'fr', 'it', 'es', 'pt', 'ja', 'zh'];
  static String text(
    String source, {
    Locale? locale,
    List<String> values = const [],
  }) {
    final code =
        locale?.languageCode ??
        (language == 'system'
            ? PlatformDispatcher.instance.locale.languageCode
            : language);
    var result = catalogs[code]?[source] ?? source;
    for (var i = 0; i < values.length; i++) {
      result = result.replaceAll('{$i}', values[i]);
    }
    return result;
  }
}

String tr(String source) => MobileStrings.text(source);

String tf(String source, List<Object?> values) => MobileStrings.text(
  source,
  values: values.map((v) => v.toString()).toList(),
);

class MobileStringsDelegate extends LocalizationsDelegate<MobileStrings> {
  const MobileStringsDelegate();
  @override
  bool isSupported(Locale locale) =>
      MobileStrings.codes.contains(locale.languageCode);
  @override
  Future<MobileStrings> load(Locale locale) async {
    if (locale.languageCode != 'en' &&
        !MobileStrings.catalogs.containsKey(locale.languageCode)) {
      final data = jsonDecode(
        await rootBundle.loadString('assets/i18n/${locale.languageCode}.json'),
      ) as Map<String, dynamic>;
      MobileStrings.catalogs[locale.languageCode] = data.map(
        (k, v) => MapEntry(k, v as String),
      );
    }
    return MobileStrings(locale);
  }

  @override
  bool shouldReload(MobileStringsDelegate old) => false;
}

/// Interface text only: website titles, URLs and account names stay ordinary Text.
class UiText extends StatelessWidget {
  const UiText(
    this.data, {
    super.key,
    this.style,
    this.textAlign,
    this.maxLines,
    this.overflow,
    this.softWrap,
    this.semanticsLabel,
    this.values = const [],
  });
  final String data;
  final TextStyle? style;
  final TextAlign? textAlign;
  final int? maxLines;
  final TextOverflow? overflow;
  final bool? softWrap;
  final String? semanticsLabel;
  final List<String> values;
  @override
  Widget build(BuildContext context) => Text(
    MobileStrings.text(
      data,
      locale: Localizations.localeOf(context),
      values: values,
    ),
    style: style,
    textAlign: textAlign,
    maxLines: maxLines,
    overflow: overflow,
    softWrap: softWrap,
    semanticsLabel: semanticsLabel,
  );
}
