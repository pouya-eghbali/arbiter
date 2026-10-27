#!/usr/bin/env python3
"""Build a checksum-pinned macOS OpenSSL runtime for the Qt deployment target."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def run(*args, cwd, env):
    print('+', subprocess.list2cmdline([str(a) for a in args]), flush=True)
    subprocess.run([str(a) for a in args], cwd=cwd, env=env, check=True)


def ensure(minimum, jobs):
    pin = json.loads((ROOT / 'tools/release/openssl.json').read_text())
    arch = platform.machine()
    if arch != 'arm64': raise RuntimeError('Portable macOS release currently targets arm64')
    work = ROOT / f'build-release-openssl/{pin["version"]}-{arch}-macos{minimum}'
    prefix = work / 'sdk'
    receipt = dict(pin, architecture=arch, minimum_os=minimum,
                   builder_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    stamp = prefix / 'arbiter-openssl-build.json'
    if stamp.exists() and json.loads(stamp.read_text()) == receipt:
        return prefix
    work.mkdir(parents=True, exist_ok=True)
    archive = work / ('openssl-' + pin['version'] + '.tar.gz')
    if not archive.exists():
        print('Downloading pinned OpenSSL source from the official release', flush=True)
        with urllib.request.urlopen(pin['url'], timeout=60) as source, archive.open('wb') as destination:
            shutil.copyfileobj(source, destination)
    with archive.open('rb') as stream: digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    if digest != pin['sha256']:
        archive.unlink()
        raise RuntimeError('OpenSSL source checksum mismatch; archive removed')
    source_root = work / 'source'
    if source_root.exists(): shutil.rmtree(source_root)
    with tarfile.open(archive) as bundle: bundle.extractall(source_root, filter='data')
    source = source_root / ('openssl-' + pin['version'])
    build = work / 'build'
    if build.exists(): shutil.rmtree(build)
    if prefix.exists(): shutil.rmtree(prefix)
    build.mkdir()
    env = dict(os.environ, MACOSX_DEPLOYMENT_TARGET=minimum,
               CFLAGS='-mmacosx-version-min=' + minimum,
               LDFLAGS='-mmacosx-version-min=' + minimum)
    run('perl', source / 'Configure', 'darwin64-arm64-cc', 'shared', 'no-legacy',
        '--prefix=' + str(prefix), '--openssldir=/etc/ssl', cwd=build, env=env)
    run('make', '-j' + str(jobs), cwd=build, env=env)
    run('make', 'test', 'TESTS=test_evp', cwd=build, env=env)
    run('make', 'install_sw', cwd=build, env=env)
    shutil.copyfile(source / 'LICENSE.txt', prefix / 'OpenSSL-LICENSE.txt')
    stamp.write_text(json.dumps(receipt, indent=2) + '\n')
    return prefix


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--minimum', required=True)
    p.add_argument('--jobs', type=int, default=12)
    a = p.parse_args()
    if not 1 <= a.jobs <= 96: p.error('--jobs must be between 1 and 96')
    print(ensure(a.minimum, a.jobs))
