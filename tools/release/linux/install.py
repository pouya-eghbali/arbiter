#!/usr/bin/env python3
"""Install an extracted Arbiter bundle for the current desktop user."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

APP_ID = 'com.arbiter.browser'
MARKER = '.arbiter-install.json'


def desktop_quote(value):
    # Desktop Exec has its own quoting rules, followed by field-code expansion.
    value = str(value).replace('%', '%%')
    for char in ('\\', '"', '`', '$'):
        value = value.replace(char, '\\' + char)
    return '"' + value.replace('\\', '\\\\') + '"'


def locations(prefix=None):
    default_prefix = (Path.home() / '.local').resolve()
    prefix = Path(prefix).expanduser().resolve() if prefix else default_prefix
    data = prefix / 'share'
    if prefix == default_prefix:
        configured = os.environ.get('XDG_DATA_HOME', '')
        if configured and Path(configured).is_absolute():
            data = Path(configured).resolve()
    paths = dict(app=data / 'arbiter', command=prefix / 'bin/arbiter',
                 desktop=data / ('applications/' + APP_ID + '.desktop'),
                 icon=data / ('icons/hicolor/scalable/apps/' + APP_ID + '.svg'))
    if any(any(c in str(p) for c in ('\n', '\r', '=', '\t')) for p in paths.values()):
        raise RuntimeError('Installation paths cannot contain line breaks, tabs, or equals signs')
    return paths


def owned(paths):
    marker = paths['app'] / MARKER
    if paths['app'].is_symlink():
        raise RuntimeError('Refusing to replace an installation directory symlink')
    if not marker.is_file():
        return False
    receipt = json.loads(marker.read_text())
    if receipt != {key: str(value) for key, value in paths.items()}:
        raise RuntimeError('Installation record does not match these installation paths')
    return True


def refresh(paths):
    for command, directory in [('update-desktop-database', paths['desktop'].parent),
                               ('gtk-update-icon-cache', paths['icon'].parents[2])]:
        if shutil.which(command):
            subprocess.run([command, '-q', str(directory)], check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def install(source, paths):
    source = source.resolve()
    required = ['arbiter', 'bin/arbiter', 'share/arbiter/arbiter.svg',
                'share/applications/' + APP_ID + '.desktop']
    if any(not (source / name).is_file() for name in required):
        raise RuntimeError('Run install.sh from a complete extracted Linux bundle')
    existing = owned(paths)
    if any(paths[key].is_symlink() for key in ('command', 'desktop', 'icon')):
        raise RuntimeError('Refusing to replace a managed file that is a symlink')
    if not existing and any(p.exists() or p.is_symlink() for p in paths.values()):
        raise RuntimeError('An installation path already exists outside this installer; refusing to overwrite it')
    if source == paths['app'] or source.is_relative_to(paths['app']) or paths['app'].is_relative_to(source):
        raise RuntimeError('The download and installation directories must be separate')
    paths['app'].parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.arbiter-install-', dir=paths['app'].parent) as temporary:
        temporary = Path(temporary)
        staged = temporary / 'app'
        shutil.copytree(source, staged, symlinks=True)
        (staged / MARKER).write_text(json.dumps({k: str(v) for k, v in paths.items()}) + '\n')
        previous = temporary / 'previous'
        if existing:
            paths['app'].rename(previous)
        try:
            staged.rename(paths['app'])
        except OSError:
            if existing:
                previous.rename(paths['app'])
            raise
    launcher = paths['app'] / 'arbiter'
    entry = (source / required[-1]).read_text()
    entry = entry.replace('Exec=arbiter %U', 'Exec=' + desktop_quote(launcher) + ' %U')
    for key in ('command', 'desktop', 'icon'):
        paths[key].parent.mkdir(parents=True, exist_ok=True)
    # A wrapper, rather than a symlink, preserves the launcher's bundle location.
    paths['command'].write_text('#!/bin/sh\nexec ' + shlex.quote(str(launcher)) + ' "$@"\n')
    paths['command'].chmod(0o755)
    paths['desktop'].write_text(entry)
    shutil.copyfile(paths['app'] / 'share/arbiter/arbiter.svg', paths['icon'])
    refresh(paths)
    print('Installed Arbiter. Open it from your application menu.')
    print('Command:', paths['command'])
    print('To uninstall: ' + shlex.join([str(paths['app'] / 'install.sh'), '--uninstall',
                                        '--prefix', str(paths['command'].parent.parent)]))
    print('Browser data stays in your existing profile; installation does not change your default browser.')


def uninstall(paths):
    if not owned(paths):
        raise RuntimeError('No installation created by this installer was found')
    for key in ('command', 'desktop', 'icon'):
        paths[key].unlink(missing_ok=True)
    shutil.rmtree(paths['app'])
    refresh(paths)
    print('Uninstalled Arbiter. Your browser profile and saved credentials were kept.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, help='Install under this prefix (default: ~/.local; no sudo needed)')
    parser.add_argument('--uninstall', action='store_true', help='Remove the installed app and desktop entry, keeping browser data')
    args = parser.parse_args()
    try:
        paths = locations(args.prefix)
        if args.uninstall:
            uninstall(paths)
        else:
            source = Path(__file__).resolve().parent
            result = subprocess.run(['/bin/sh', str(source / 'arbiter'), '--check-dependencies'], check=False)
            if result.returncode:
                raise RuntimeError('System library check failed; choose a matching build or install its distribution dependencies first')
            install(source, paths)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'Installation failed: {error}\n')


if __name__ == '__main__':
    main()
