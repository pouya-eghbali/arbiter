#!/usr/bin/env python3
"""Build a native DEB against the build distribution's installed library packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from package import audit, elf_type, linux_external_libraries, normalize_linux_permissions
import linux_abi


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('Build on the matching Debian/Ubuntu distribution with dpkg-dev installed')
    bundle, output = args.bundle.resolve(), args.output.resolve()
    metadata = json.loads((bundle / 'share/arbiter/release.json').read_text())
    version = metadata['version']
    if metadata['platform'] != 'linux' or metadata['architecture'] != 'x86_64' or not re.fullmatch(r'\d+\.\d+\.\d+', version):
        parser.error('Expected a stable Linux x86_64 release bundle')
    needed = subprocess.check_output(['patchelf', '--print-needed', str(bundle / 'lib/libQt6WebEngineCore.so.6')], text=True)
    abi = linux_abi.identify(needed)
    if metadata.get('linux_ffmpeg') != abi:
        parser.error('Bundle label disagrees with actual FFmpeg dependencies')
    for library, major in abi['libraries'].items():
        status = subprocess.check_output(['dpkg-query', '-W', '-f=${db:Status-Status}', library + str(major)], text=True)
        if status != 'installed':
            parser.error('Build in a distribution providing the matching FFmpeg packages')
    audit(bundle); linux_external_libraries(bundle)
    output.mkdir(parents=True, exist_ok=True)
    target = output / f'arbiter_{version}-1.ffmpeg{abi["major"]}_amd64.deb'
    if target.exists():
        parser.error('Refusing to overwrite ' + str(target))
    with tempfile.TemporaryDirectory(prefix='arbiter-deb-') as temporary:
        work = Path(temporary); stage = work / 'package'
        app = stage / 'opt/arbiter'; app.parent.mkdir(parents=True)
        shutil.copytree(bundle, app, symlinks=True)
        normalize_linux_permissions(app)
        (stage / 'usr/bin').mkdir(parents=True)
        command = stage / 'usr/bin/arbiter'
        command.write_text('#!/bin/sh\nexec /opt/arbiter/arbiter "$@"\n'); command.chmod(0o755)
        entry = stage / 'usr/share/applications/com.arbiter.browser.desktop'; entry.parent.mkdir(parents=True)
        entry.write_text((app / 'share/applications/com.arbiter.browser.desktop').read_text()
                         .replace('Exec=arbiter %U', 'Exec=/opt/arbiter/arbiter %U'))
        icon = stage / 'usr/share/icons/hicolor/scalable/apps/com.arbiter.browser.svg'; icon.parent.mkdir(parents=True)
        shutil.copy2(app / 'share/arbiter/arbiter.svg', icon)
        control = stage / 'DEBIAN'; control.mkdir()
        (work / 'debian').mkdir()
        source_control = 'Source: arbiter\nMaintainer: Arbiter contributors <noreply@arbiterbrowser.com>\n\nPackage: arbiter\nArchitecture: amd64\nDescription: A browser for power users\n'
        (work / 'debian/control').write_text(source_control)
        binaries = [str(p) for p in app.rglob('*') if p.is_file() and not p.is_symlink() and elf_type(p) in (2, 3)]
        result = subprocess.check_output(['dpkg-shlibdeps', '-O', '--ignore-missing-info',
            '-l' + str(app / 'lib'), *['-e' + p for p in binaries]], cwd=work, text=True)
        dependencies = next(line.split('=', 1)[1] for line in result.splitlines() if line.startswith('shlibs:Depends='))
        for library, major in abi['libraries'].items():
            if not re.search(r'(?:^|,\s*)' + re.escape(library + str(major)) + r'(?:\s|,|$)', dependencies):
                raise RuntimeError('Missing FFmpeg package dependency: ' + library)
        (control / 'control').write_text(f'Package: arbiter\nVersion: {version}-1.ffmpeg{abi["major"]}\nArchitecture: amd64\nSection: web\nPriority: optional\nMaintainer: Arbiter contributors <noreply@arbiterbrowser.com>\nHomepage: https://arbiterbrowser.com\nDepends: {dependencies}, hicolor-icon-theme\nDescription: A browser for power users\n Open source browser with workspaces, split views, and a floating navigation pill.\n')
        subprocess.run(['dpkg-deb', '--root-owner-group', '-Zxz', '-z3', '--threads-max=8', '--build', str(stage), str(target)], check=True)
    with target.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    target.with_name(target.name + '.sha256').write_text(digest + '  ' + target.name + '\n')
    print(target)


if __name__ == '__main__':
    main()
