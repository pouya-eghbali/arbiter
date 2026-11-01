#!/usr/bin/env python3
"""Align/sign a locally built Release APK. Never upload; passwords come from env."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir', type=Path, required=True)
    p.add_argument('--sdk', type=Path, required=True)
    p.add_argument('--build-tools', default='36.0.0')
    p.add_argument('--keystore', type=Path, required=True)
    p.add_argument('--alias', required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    build = a.build_dir.expanduser().resolve()
    cache = (build / 'CMakeCache.txt').read_text()
    if not re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M):
        p.error('Use build.py android --release first; a Debug APK is not a release artifact')
    apk = build / 'android-build/arbiter_mobile.apk'
    tools = a.sdk.expanduser().resolve() / 'build-tools' / a.build_tools
    if not apk.is_file() or not a.keystore.expanduser().is_file():
        p.error('APK or keystore does not exist')
    if not os.environ.get('ARBITER_ANDROID_STORE_PASSWORD') or not os.environ.get('ARBITER_ANDROID_KEY_PASSWORD'):
        p.error('Set ARBITER_ANDROID_STORE_PASSWORD and ARBITER_ANDROID_KEY_PASSWORD (not command-line passwords)')
    suffix = '.exe' if os.name == 'nt' else ''
    java_home = os.environ.get('JAVA_HOME')
    java = str(Path(java_home) / 'bin' / ('java' + suffix)) if java_home else shutil.which('java')
    if not java:
        p.error('Set JAVA_HOME or put a supported JDK on PATH')
    signer = [java, '-jar', str(tools / 'lib/apksigner.jar')]
    manifest = subprocess.check_output([str(tools / ('aapt' + suffix)), 'dump', 'xmltree', str(apk), 'AndroidManifest.xml'], text=True)
    if re.search(r'android:debuggable[^\n]*0xffffffff', manifest):
        p.error('The APK is debuggable; rebuild the apk target in Release configuration')
    output = a.output.expanduser().resolve()
    if output.exists():
        p.error('Output already exists; choose a new filename')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Work beside output so the final replacement is atomic on the same volume.
    with tempfile.TemporaryDirectory(prefix='arbiter-sign-', dir=output.parent) as folder:
        aligned = Path(folder) / 'aligned.apk'
        signed = Path(folder) / 'signed.apk'
        subprocess.run([str(tools / ('zipalign' + suffix)), '-P', '16', '-f', '4', str(apk), str(aligned)], check=True)
        subprocess.run(signer + ['sign', '--ks', str(a.keystore.expanduser().resolve()),
                        '--ks-key-alias', a.alias, '--ks-pass', 'env:ARBITER_ANDROID_STORE_PASSWORD',
                        '--key-pass', 'env:ARBITER_ANDROID_KEY_PASSWORD', '--out', str(signed), str(aligned)], check=True)
        subprocess.run(signer + ['verify', '--verbose', str(signed)], check=True)
        subprocess.run([str(tools / ('zipalign' + suffix)), '-c', '-P', '16', '4', str(signed)], check=True)
        signed.replace(output)
    with output.open('rb') as file:
        digest = hashlib.file_digest(file, 'sha256').hexdigest()
    output.with_suffix('.apk.sha256').write_text(f'{digest}  {output.name}\n')
    print('Signed APK:', output)


if __name__ == '__main__':
    main()
