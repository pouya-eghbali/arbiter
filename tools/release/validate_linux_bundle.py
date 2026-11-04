#!/usr/bin/env python3
"""Validate an extracted native bundle on the target Linux distribution."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import re
import select
import shutil

import linux_abi


def run(*args, env):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), env=env, check=True, timeout=120)


@contextmanager
def headless_display(env):
    # Tumbleweed provides Xvfb separately from the optional xvfb-run wrapper.
    read_fd, write_fd = os.pipe()
    process = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0', '1024x768x24', '-nolisten', 'tcp'],
                               pass_fds=(write_fd,))
    os.close(write_fd)
    try:
        if not select.select([read_fd], [], [], 20)[0]:
            raise RuntimeError('Xvfb did not start')
        display = os.read(read_fd, 64).strip().decode()
        if not display.isdigit():
            raise RuntimeError('Xvfb did not allocate a display')
        yield dict(env, DISPLAY=':' + display)
    finally:
        os.close(read_fd)
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill(); process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    args = parser.parse_args()
    if not sys.platform.startswith('linux') or os.geteuid() == 0:
        parser.error('Run on Linux as a regular desktop user, not root')
    bundle = args.bundle.resolve()
    metadata = json.loads((bundle / 'share/arbiter/release.json').read_text())
    core = bundle / 'lib/libQt6WebEngineCore.so.6'
    if shutil.which('patchelf'):
        needed = subprocess.check_output(['patchelf', '--print-needed', str(core)], text=True)
    else:
        linked = subprocess.check_output(['ldd', str(core)], text=True,
            env=dict(os.environ, LD_LIBRARY_PATH=str(bundle / 'lib')))
        needed = '\n'.join(sorted(set(re.findall(r'\b(?:libavcodec|libavformat|libavutil)\.so\.\d+', linked))))
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
        with headless_display(env) as display_env:
            if shutil.which('dbus-run-session'):
                run('dbus-run-session', '--', command, '--release-smoke-test', env=display_env)
            else:
                run(command, '--release-smoke-test', env=display_env)
        run('/bin/sh', root / '.local/share/arbiter/install.sh', '--uninstall', env=env)
        assert profile.read_text() == 'keep'
        assert not command.exists() and not (root / '.local/share/arbiter').exists()
    print('Validated FFmpeg', actual['major'], 'bundle: system libraries, desktop entry, installed startup, and uninstall.')


if __name__ == '__main__':
    main()
