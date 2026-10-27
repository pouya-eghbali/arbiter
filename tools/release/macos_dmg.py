#!/usr/bin/env python3
"""Wrap a finalized, notarized Arbiter app in a signed drag-to-Applications DMG."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def create_dmg(app, output, identity, profile):
    if sys.platform != 'darwin':
        raise RuntimeError('DMG packaging requires macOS')
    if not identity or not profile:
        raise RuntimeError('Signing identity and notarization keychain profile are required')
    app, output = Path(app).resolve(), Path(output).resolve()
    metadata = json.loads((app / 'Contents/Resources/release.json').read_text())
    if metadata['platform'] != 'macos' or not metadata['signed']:
        raise RuntimeError('Use a finalized signed macOS release app')
    expected = f'arbiter-{metadata["version"]}-macos-{metadata["architecture"]}.dmg'
    if output.name != expected or output.exists():
        raise RuntimeError('Output must be a new file named ' + expected)
    from ds_store import DSStore
    run('codesign', '--verify', '--deep', '--strict', app)
    run('xcrun', 'stapler', 'validate', app)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='arbiter-dmg-') as temporary:
        stage = Path(temporary) / 'contents'
        stage.mkdir()
        run('ditto', app, stage / 'Arbiter.app')
        (stage / 'Applications').symlink_to('/Applications')
        with DSStore.open(str(stage / '.DS_Store'), 'w+') as store:
            store['.']['bwsp'] = {
                'ShowStatusBar': False, 'ShowToolbar': False, 'ShowSidebar': False,
                'ShowPathbar': False, 'ShowTabView': False,
                'WindowBounds': '{{200, 200}, {640, 360}}',
            }
            store['.']['icvp'] = {
                'viewOptionsVersion': 1, 'iconSize': 112.0, 'textSize': 14.0,
                'labelOnBottom': True, 'arrangeBy': 'none', 'showIconPreview': True,
                'showItemInfo': False, 'backgroundType': 0,
                'gridOffsetX': 0.0, 'gridOffsetY': 0.0, 'gridSpacing': 100.0,
            }
            store['.']['vstl'] = ('type', b'icnv')
            store['Arbiter.app']['Iloc'] = (160, 160)
            store['Applications']['Iloc'] = (480, 160)
        run('hdiutil', 'create', '-volname', 'Arbiter', '-srcfolder', stage,
            '-format', 'UDZO', output)
    run('codesign', '--force', '--sign', identity, '--timestamp', output)
    result = subprocess.check_output([
        'xcrun', 'notarytool', 'submit', str(output), '--keychain-profile', profile,
        '--wait', '--output-format', 'json',
    ], text=True)
    if json.loads(result).get('status') != 'Accepted':
        raise RuntimeError('Apple did not accept the disk image for notarization')
    run('xcrun', 'stapler', 'staple', output)
    run('xcrun', 'stapler', 'validate', output)
    run('codesign', '--verify', '--strict', output)
    run('spctl', '--assess', '--type', 'open', '--context', 'context:primary-signature', output)
    with output.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    output.with_name(output.name + '.sha256').write_text(digest + '  ' + output.name + '\n')
    print(output)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    create_dmg(args.app, args.output, os.environ.get('ARBITER_MACOS_SIGNING_IDENTITY'),
               os.environ.get('ARBITER_NOTARY_PROFILE'))
