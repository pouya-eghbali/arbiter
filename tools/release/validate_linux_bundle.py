#!/usr/bin/env python3
"""Validate an extracted native bundle on the target Linux distribution."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import linux_abi


def run(*args, env):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), env=env, check=True, timeout=120)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    args = parser.parse_args()
    if not sys.platform.startswith('linux') or os.geteuid() == 0:
        parser.error('Run on Linux as a regular desktop user, not root')
    bundle = args.bundle.resolve()
    metadata = json.loads((bundle / 'share/arbiter/release.json').read_text())
    needed = subprocess.check_output(['patchelf', '--print-needed', str(bundle / 'lib/libQt6WebEngineCore.so.6')], text=True)
    actual = linux_abi.identify(needed)
    if metadata.get('linux_ffmpeg') != actual:
        parser.error('Engine ELF dependencies disagree with the package ABI metadata')
    for file in bundle.rglob('*'):
        if file.is_file() and file.name.startswith(('libavcodec', 'libavformat', 'libavutil', 'libswresample', 'libswscale')):
            parser.error('A codec library was bundled: ' + str(file))
    env = {k: v for k, v in os.environ.items() if not k.startswith(('QT_', 'QML', 'QTWEBENGINE_'))
           and k not in ('LD_LIBRARY_PATH', 'LD_PRELOAD')}
    with tempfile.TemporaryDirectory(prefix='arbiter-linux-install-test-') as folder:
        root = Path(folder)
        env.update(HOME=folder, XDG_DATA_HOME=str(root / '.local/share'),
            XDG_CONFIG_HOME=str(root / '.config'), XDG_CACHE_HOME=str(root / '.cache'),
            QT_QUICK_BACKEND='software', QT_QPA_PLATFORM='xcb')
        profile = root / '.config/keep-profile'; profile.parent.mkdir(); profile.write_text('keep')
        run('/bin/sh', bundle / 'install.sh', env=env)
        command = root / '.local/bin/arbiter'
        run(command, '--check-dependencies', env=env)
        run('desktop-file-validate', root / '.local/share/applications/com.arbiter.browser.desktop', env=env)
        run('dbus-run-session', '--', 'xvfb-run', '-a', command, '--release-smoke-test', env=env)
        run('/bin/sh', root / '.local/share/arbiter/install.sh', '--uninstall', env=env)
        assert profile.read_text() == 'keep'
        assert not command.exists() and not (root / '.local/share/arbiter').exists()
    print('Validated FFmpeg', actual['major'], 'bundle: system libraries, desktop entry, installed startup, and uninstall.')


if __name__ == '__main__':
    main()
