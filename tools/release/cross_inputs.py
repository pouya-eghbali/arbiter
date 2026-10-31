#!/usr/bin/env python3
"""Validate/package a precompiled engine on its target OS, never rebuild Chromium."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import stat
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build-cross-validation'


def download():
    platform = os.environ['INPUT_PLATFORM']
    tag, digest = os.environ['RELEASE_TAG'], os.environ['INPUT_SHA256']
    if platform not in ('windows', 'linux') or not re.fullmatch(r'v\d+\.\d+\.\d+', tag) or not re.fullmatch(r'[0-9a-f]{64}', digest):
        raise RuntimeError('Invalid cross-build input request')
    repo = os.environ['GITHUB_REPOSITORY']
    revision = subprocess.check_output(['gh', 'api', f'repos/{repo}/commits/{tag}', '--jq', '.sha'], text=True).strip()
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    if revision != head:
        raise RuntimeError('Build inputs must belong to the exact checked-out release commit')
    WORK.mkdir(exist_ok=True)
    asset = f'build-inputs-{platform}.zip'
    subprocess.run(['gh', 'release', 'download', tag, '--repo', repo, '--pattern', asset,
                    '--dir', str(WORK), '--clobber'], check=True)
    archive = WORK / asset
    with archive.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != digest:
            raise RuntimeError('Cross-build input archive checksum mismatch')
    with zipfile.ZipFile(archive) as bundle:
        seen = set()
        for member in bundle.infolist():
            path = PurePosixPath(member.filename)
            mode = member.external_attr >> 16
            if (path.is_absolute() or '..' in path.parts or '\\' in member.filename
                    or PureWindowsPath(member.filename).drive or not path.parts
                    or path.parts[0] not in ('sdk', 'openssl') or member.filename in seen
                    or stat.S_ISLNK(mode)):
                raise RuntimeError('Unsafe cross-build archive member')
            seen.add(member.filename)
        bundle.extractall(WORK)
    (WORK / 'source-commit').write_text(head + '\n')


def build():
    platform = os.environ['INPUT_PLATFORM']
    if (platform == 'windows') != (sys.platform == 'win32') or (platform == 'linux') != sys.platform.startswith('linux'):
        raise RuntimeError('Cross SDK validation must run on its target platform')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    if (WORK / 'source-commit').read_text().strip() != head:
        raise RuntimeError('Cross inputs were downloaded for another source revision')
    deps = json.loads((ROOT / 'dependencies.json').read_text())
    subprocess.run([sys.executable, '-m', 'pip', 'install', 'html5lib==' + deps['html5lib']], check=True)
    env = os.environ.copy()
    if platform == 'windows':
        env['ARBITER_OPENSSL_ROOT'] = str(WORK / 'openssl')
        # Do not let Windows loader errors open an unattended modal dialog.
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    subprocess.run([sys.executable, str(ROOT / 'tools/release/build.py'), '--package', '--unsigned',
                    '--jobs', '4', '--sdk-prefix', str(WORK / 'sdk')], check=True, env=env)


if __name__ == '__main__':
    if len(sys.argv) != 2 or sys.argv[1] not in ('download', 'build'):
        raise SystemExit('Usage: cross_inputs.py download|build')
    globals()[sys.argv[1]]()
