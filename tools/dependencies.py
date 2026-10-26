#!/usr/bin/env python3
"""Inspect release pins or propose a coordinated Qt/WebEngine/Chromium update.

Uses only upstream Qt repositories. Proposals never rewrite or auto-rebase patches;
engine/build.py --prepare-only and the native SDK pipeline must validate them.
"""
import argparse
import base64
import json
import os
from pathlib import Path
import re
import sys
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'engine'))
from identity import fingerprint


def version(value):
    if not re.fullmatch(r'\d+\.\d+\.\d+', value):
        raise ValueError(f'Expected a stable X.Y.Z version, got {value!r}')
    return tuple(map(int, value.split('.')))


def api(path):
    headers = {'Accept': 'application/vnd.github+json', 'User-Agent': 'Arbiter-dependency-updater'}
    if os.environ.get('GH_TOKEN'): headers['Authorization'] = 'Bearer ' + os.environ['GH_TOKEN']
    with urllib.request.urlopen(urllib.request.Request('https://api.github.com/repos/qt/' + path, headers=headers), timeout=60) as response:
        return json.load(response)


def content(repo, path, ref):
    item = api(repo + '/contents/' + path + '?ref=' + urllib.parse.quote(ref, safe=''))
    return base64.b64decode(item['content']).decode()


def latest(repo, current):
    candidates = []
    # Qt's newest stable releases are in the first 100 tags; pre-releases excluded.
    for tag in api(repo + '/tags?per_page=100'):
        value = tag['name'].removeprefix('v')
        if (re.fullmatch(r'\d+\.\d+\.\d+', value) and value.startswith('6.')
                and version(value) >= version(current)):
            candidates.append(value)
    return max(candidates, key=version) if candidates else None


def propose(manifest, qt, ref):
    version(qt)
    encoded = urllib.parse.quote(ref, safe='')
    commit = api('qtwebengine/commits/' + encoded)['sha']
    config = content('qtwebengine', '.cmake.conf', commit)
    match = re.search(r'set\(QT_REPO_MODULE_VERSION "([\d.]+)"\)', config)
    if not match: raise RuntimeError('Cannot read upstream Qt WebEngine version')
    prerelease = re.search(r'set\(QT_REPO_MODULE_PRERELEASE_VERSION_SEGMENT "([^"]+)"\)', config)
    if prerelease: raise RuntimeError('Refusing an upstream prerelease; select a stable release tag')
    submodule = api('qtwebengine/contents/src/3rdparty?ref=' + commit)
    chromium = submodule['sha']
    lines = content('qtwebengine-chromium', 'chromium/chrome/VERSION', chromium)
    values = dict(line.split('=', 1) for line in lines.splitlines() if '=' in line)
    result = dict(manifest, qt=qt, webengine_version=match[1], webengine_commit=commit,
                  chromium_commit=chromium,
                  chromium_version='.'.join(values[k] for k in ('MAJOR', 'MINOR', 'BUILD', 'PATCH')))
    # These two version schemes differ. The native configure/build is the final
    # compatibility gate, not numeric equality of Qt and WebEngine versions.
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    sub.add_parser('metadata')
    update = sub.add_parser('update')
    update.add_argument('--qt', help='Exact base Qt version; latest stable if omitted')
    update.add_argument('--webengine-ref', help='Upstream stable tag; latest stable if omitted')
    update.add_argument('--dry-run', action='store_true', help='Report candidate pins without modifying files')
    a = p.parse_args()
    path = ROOT / 'engine/manifest.json'
    manifest = json.loads(path.read_text())
    deps = json.loads((ROOT / 'dependencies.json').read_text())
    if a.command == 'metadata':
        print(json.dumps({'qt': manifest['qt'], 'python': deps['python'], 'rust': deps['rust'], 'html5lib': deps['html5lib'],
                          'engine_key': fingerprint(), 'version': (ROOT / 'VERSION.txt').read_text().strip()}))
        return
    qt = a.qt or latest('qtbase', manifest['qt']) or manifest['qt']
    ref = a.webengine_ref
    if ref is None:
        newest = latest('qtwebengine', manifest['webengine_version'])
        if newest is None and qt == manifest['qt']:
            print('Qt/WebEngine pins are already at or above the latest stable release.')
            return
        if newest is not None: ref = 'v' + newest
    candidate = propose(manifest, qt, ref) if ref else dict(manifest, qt=qt)
    for key in ('qt', 'webengine_version', 'chromium_version'):
        if version(candidate[key]) < version(manifest[key]):
            raise RuntimeError(f'Refusing to downgrade {key}')
    if candidate != manifest:
        if not a.dry_run: path.write_text(json.dumps(candidate, indent=2) + '\n')
        print(json.dumps({'before': manifest, 'candidate': candidate}, indent=2))
        print('Candidate only: validate patch application and all native builds before merging.')


if __name__ == '__main__': main()
