#!/usr/bin/env python3
"""Extract static Qt/native text and reuse offline catalogs. No network calls."""
import argparse
import collections
import json
from pathlib import Path
import plistlib
import re
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
folder = root / 'mobile/assets/i18n'
codes = ['en', 'de', 'fr', 'it', 'es', 'pt', 'ja', 'zh']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--check', action='store_true')
a = p.parse_args()
# Only literal UI strings, never page content, secrets, or identifiers.
quoted = r'"((?:[^"\\]|\\.)*)"'
keys = set()
for directory in ['mobile/qml', 'mobile/src', 'mobile/platform']:
    for path in (root / directory).rglob('*'):
        if path.suffix not in {'.qml', '.cpp', '.mm', '.swift', '.kt'} or 'tests' in path.parts:
            continue
        text = path.read_text()
        patterns = [r'(?:qsTr|\btr|ArbiterText)\(\s*' + quoted,
                    r'MobileStrings\.text\([^,\n]+,\s*' + quoted,
                    r'MobileStrings\.text\([^,\n]+,if\([^)]*\)\s*' + quoted + r'\s*else\s*' + quoted,
                    r'ArbiterText\([^\n?]+\?\s*' + quoted,
                    r'ArbiterText\([^\n?]+\?\s*' + quoted + r'\s*:\s*' + quoted]
        for pattern in patterns:
            for match in re.finditer(pattern, text):
                try:
                    for literal in match.groups():
                        keys.add(json.loads('"' + literal + '"'))
                except json.JSONDecodeError:
                    raise ValueError(f'Unsupported escape in {path}: {match.group(1)}')
for owner in ['App', 'Provider']:
    info = plistlib.loads((root / f'mobile/platform/ios/{owner}-Info.plist').read_bytes())
    keys.update(v for k, v in info.items() if k.endswith('UsageDescription') and isinstance(v, str))
for code in codes:
    path = folder / f'{code}.json'
    old = json.loads(path.read_text())
    previous = json.loads(subprocess.check_output(['git', 'show', f'HEAD:mobile/assets/i18n/{code}.json'], cwd=root))
    old = previous | {k: v for k, v in old.items() if v != k}
    desktop = {}
    for ts in (root / 'translations').glob(f'*_{code}*.ts'):
        for message in ET.parse(ts).iter('message'):
            source = message.findtext('source')
            translated = message.find('translation')
            if translated is not None and translated.get('type') != 'unfinished' and translated.text:
                desktop[source] = translated.text
    data = {key: key if code == 'en' else old.get(key, desktop.get(key, key)) for key in sorted(keys)}
    for key, value in data.items():
        if not value.strip() or collections.Counter(re.findall(r'\{\d+\}|%\d+', key)) != collections.Counter(re.findall(r'\{\d+\}|%\d+', value)):
            raise ValueError(f'{code}: invalid translation: {key}')
    if a.check:
        if data != json.loads(path.read_text()):
            raise ValueError(f'{code}: catalogs need regeneration')
    else:
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')
        for owner in ['App', 'Provider']:
            info = plistlib.loads((root / f'mobile/platform/ios/{owner}-Info.plist').read_bytes())
            values = {k: data[v] for k, v in info.items() if k.endswith('UsageDescription') and isinstance(v, str)}
            out = root / f'mobile/platform/ios/{owner}Localization/{code}.lproj/InfoPlist.strings'
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text('\n'.join(json.dumps(k) + ' = ' + json.dumps(v, ensure_ascii=False) + ';' for k, v in values.items()) + '\n')
    fallback = sum(v == k for k, v in data.items()) if code != 'en' else 0
    print(f'{code}: {len(data)} messages, {fallback} English fallbacks')
