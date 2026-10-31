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
    release = json.loads(subprocess.check_output(['gh', 'api', f'repos/{repo}/releases/tags/{tag}'], text=True))
    revision = release.get('target_commitish', '')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    if not release.get('draft') or not re.fullmatch(r'[0-9a-f]{40}', revision) or revision != head:
        raise RuntimeError('Build inputs must belong to a draft pinned to the exact checked-out release commit')
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
                    or PureWindowsPath(member.filename).drive or ':' in member.filename or not path.parts
                    or path.parts[0] not in ('sdk', 'openssl') or member.filename in seen
                    or stat.S_ISLNK(mode)):
                raise RuntimeError('Unsafe cross-build archive member')
            seen.add(member.filename)
        bundle.extractall(WORK)
        for member in bundle.infolist():
            mode = (member.external_attr >> 16) & 0o777
            if mode:
                (WORK / member.filename).chmod(mode)
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


def pack():
    platform = os.environ['INPUT_PLATFORM']
    if platform not in ('windows', 'linux'):
        raise RuntimeError('Expected windows or linux cross-build inputs')
    work = Path(os.environ.get('ARBITER_CROSS_WORK', str(ROOT / ('build-' + platform + '-cross')))).resolve()
    sys.path.insert(0, str(ROOT / 'engine'))
    from identity import verify_sdk
    receipt = verify_sdk(work / 'sdk')
    if receipt.get('platform') != {'windows': 'win32', 'linux': 'linux'}[platform] or receipt.get('architecture') != 'x86_64':
        raise RuntimeError('Cross-build inputs have the wrong platform/architecture')
    roots = [('sdk', work / 'sdk')]
    if platform == 'windows':
        openssl = work / 'openssl/sdk'
        recorded = json.loads((openssl / 'arbiter-openssl-build.json').read_text())
        expected = json.loads((ROOT / 'tools/release/openssl.json').read_text())
        if recorded.get('linkage') != 'static' or any(recorded.get(k) != v for k,v in expected.items()):
            raise RuntimeError('Cross OpenSSL SDK does not match release pins')
        roots.append(('openssl', openssl))
    output = ROOT / 'build-release-artifacts' / ('build-inputs-' + platform + '.zip')
    output.parent.mkdir(exist_ok=True)
    temporary = output.with_suffix('.zip.partial')
    with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1) as bundle:
        for name, folder in roots:
            for file in sorted(folder.rglob('*')):
                if not file.resolve().is_relative_to(folder.resolve()):
                    raise RuntimeError('SDK link escapes its root: ' + str(file))
                if file.is_file():
                    # Dereference contained shared-library SONAME links. The
                    # receiver rejects symlinks and restores executable modes.
                    bundle.write(file, name + '/' + file.relative_to(folder).as_posix())
    temporary.replace(output)
    with output.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    print(json.dumps(dict(asset=str(output), sha256=digest, engine=receipt['inputs_sha256'])))


if __name__ == '__main__':
    if len(sys.argv) != 2 or sys.argv[1] not in ('download', 'build', 'pack'):
        raise SystemExit('Usage: cross_inputs.py download|build|pack')
    globals()[sys.argv[1]]()
