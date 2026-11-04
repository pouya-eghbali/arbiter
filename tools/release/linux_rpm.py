#!/usr/bin/env python3
"""Build a native x64 RPM from an already validated, codec-free Linux bundle."""
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

from package import audit, linux_external_libraries
import linux_abi


def dependencies(bundle):
    external = linux_external_libraries(bundle)
    if any(not re.fullmatch(r'[A-Za-z0-9_+.-]+\.so(?:\.\d+)*', name) for name in external):
        raise RuntimeError('Nonportable dependency name: ' + repr(external))
    return sorted(name + '()(64bit)' for name in external)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('Build the RPM on Linux with rpmbuild and patchelf installed')
    bundle, output = args.bundle.resolve(), args.output.resolve()
    metadata = json.loads((bundle / 'share/arbiter/release.json').read_text())
    if metadata['platform'] != 'linux' or metadata['architecture'] != 'x86_64':
        parser.error('Expected a Linux x86_64 bundle')
    version = metadata['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        parser.error('RPM releases require a stable X.Y.Z version')
    needed = subprocess.check_output(['patchelf', '--print-needed', str(bundle / 'lib/libQt6WebEngineCore.so.6')], text=True)
    abi = linux_abi.identify(needed)
    if metadata.get('linux_ffmpeg') != abi:
        parser.error('Bundle label disagrees with actual FFmpeg dependencies')
    audit(bundle)
    requirements = dependencies(bundle)
    requirements += ['libssl.so.3()(64bit)', 'libc.so.6(GLIBC_2.39)(64bit)']
    with tempfile.TemporaryDirectory(prefix='arbiter-rpm-') as temporary:
        root = Path(temporary)
        spec = root / 'arbiter.spec'
        spec.write_text(f'''Name: arbiter
Version: {version}
Release: 1.ffmpeg{abi['major']}
Summary: A browser for power users
License: GPL-3.0-only
URL: https://arbiterbrowser.com
BuildArch: x86_64
AutoReqProv: no
Requires: {', '.join(sorted(set(requirements)))}
%description
Arbiter is an open source browser with workspaces, split views, and a floating
navigation pill. This package uses the distribution's FFmpeg {abi['major']} libraries.

%install
mkdir -p "%{{buildroot}}/opt/arbiter" "%{{buildroot}}/usr/bin" "%{{buildroot}}/usr/share/applications" "%{{buildroot}}/usr/share/icons/hicolor/scalable/apps"
cp -a "$ARBITER_RPM_BUNDLE/." "%{{buildroot}}/opt/arbiter/"
printf '#!/bin/sh\\nexec /opt/arbiter/arbiter "$@"\\n' > "%{{buildroot}}/usr/bin/arbiter"
chmod 755 "%{{buildroot}}/usr/bin/arbiter"
sed 's|^Exec=arbiter %U$|Exec=/opt/arbiter/arbiter %U|' "$ARBITER_RPM_BUNDLE/share/applications/com.arbiter.browser.desktop" > "%{{buildroot}}/usr/share/applications/com.arbiter.browser.desktop"
cp "$ARBITER_RPM_BUNDLE/share/arbiter/arbiter.svg" "%{{buildroot}}/usr/share/icons/hicolor/scalable/apps/com.arbiter.browser.svg"

%files
%defattr(-,root,root,-)
/opt/arbiter
/usr/bin/arbiter
/usr/share/applications/com.arbiter.browser.desktop
/usr/share/icons/hicolor/scalable/apps/com.arbiter.browser.svg
''')
        env = dict(os.environ, ARBITER_RPM_BUNDLE=str(bundle))
        subprocess.run(['rpmbuild', '-bb', str(spec), '--define', '_topdir ' + str(root),
            '--define', '_build_id_links none', '--define', 'debug_package %{nil}',
            '--define', '__os_install_post %{nil}', '--define', '_binary_payload w3.zstdio'],
            env=env, check=True)
        packages = list((root / 'RPMS/x86_64').glob('arbiter-*.rpm'))
        if len(packages) != 1:
            raise RuntimeError('Expected one RPM')
        output.mkdir(parents=True, exist_ok=True)
        target = output / packages[0].name
        if target.exists():
            raise RuntimeError('Refusing to overwrite ' + str(target))
        shutil.copy2(packages[0], target)
    with target.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    target.with_name(target.name + '.sha256').write_text(digest + '  ' + target.name + '\n')
    print(target)


if __name__ == '__main__':
    main()
