#!/usr/bin/env python3
"""Validate bundled TS catalogs without a translation service or Qt runtime."""
from collections import Counter
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
CATALOGS = ROOT / 'translations'
PLACEHOLDER = re.compile(r'%L?\d+|%n')
URL = re.compile(r'https?://[^\s]+')
errors = []
reference = None
for language in json.loads((CATALOGS / 'languages.json').read_text(encoding='utf-8')):
    code = language['code']
    if code == 'en':
        continue
    path = CATALOGS / f'arbiter_{code}.ts'
    root = ET.parse(path).getroot()
    if root.get('language') != code:
        errors.append(f'{path.name}: incorrect language attribute')
    keys = set()
    translated = 0
    for context in root.findall('context'):
        name = context.findtext('name')
        for message in context.findall('message'):
            source = message.findtext('source') or ''
            translation = message.find('translation')
            key = (name, source, message.findtext('comment'))
            if key in keys:
                errors.append(f'{path.name}: duplicate message {key}')
            keys.add(key)
            if translation is None or translation.get('type') in {'unfinished', 'vanished', 'obsolete'}:
                errors.append(f'{path.name}: missing translation: {key}')
                continue
            forms = translation.findall('numerusform') or [translation]
            for form in forms:
                text = ''.join(form.itertext())
                if not text.strip():
                    errors.append(f'{path.name}: empty translation: {key}')
                if Counter(PLACEHOLDER.findall(source)) != Counter(PLACEHOLDER.findall(text)):
                    errors.append(f'{path.name}: altered placeholders: {key}')
                if URL.findall(source) != URL.findall(text):
                    errors.append(f'{path.name}: altered URL: {key}')
                for brand in ('Arbiter', 'Widevine', 'Touch ID', 'Windows Hello', 'TOML', 'WebAuthn'):
                    if source.count(brand) != text.count(brand):
                        errors.append(f'{path.name}: altered brand {brand}: {key}')
                if 'ZXQ' in text or re.search(r'\[\[\d+\]\]', text):
                    errors.append(f'{path.name}: draft generation marker: {key}')
            translated += 1
    if reference is None:
        reference = keys
    elif keys != reference:
        errors.append(f'{path.name}: message set differs from other catalogs')
    print(f'{code}: {translated}/{len(keys)} translated messages')
if errors:
    print('\n'.join(errors), file=sys.stderr)
    sys.exit(1)
print('All catalogs have matching messages, placeholders, brands, and URLs.')
