#!/usr/bin/env python3
"""Build Linux x64 in Docker without exposing local secrets to the container."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def run(*args, capture=False):
    result = subprocess.run([str(a) for a in args], cwd=ROOT, check=True, text=True,
                            stdout=subprocess.PIPE if capture else None)
    return result.stdout.strip() if capture else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=24)
    parser.add_argument('--allow-emulation', action='store_true', help='Allow slow Linux x64 emulation on an ARM Docker engine')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 96: parser.error('--jobs must be between 1 and 96')
    from package import require_clean_source
    require_clean_source()
    architecture = run('docker', 'info', '--format', '{{.Architecture}}', capture=True)
    if architecture not in ('x86_64', 'amd64') and not args.allow_emulation:
        raise SystemExit('Use an x64 Docker engine, or explicitly pass --allow-emulation on ARM.')
    uid = os.environ.get('ARBITER_DOCKER_UID', str(os.getuid()) if hasattr(os, 'getuid') else '1000')
    gid = os.environ.get('ARBITER_DOCKER_GID', str(os.getgid()) if hasattr(os, 'getgid') else '1000')
    if uid == '0': raise SystemExit('Run as a regular user; Chromium release tests require a non-root container user.')
    context = ROOT / 'build-release-docker/context'
    context.mkdir(parents=True, exist_ok=True)
    for name in ('Dockerfile', 'entrypoint.sh', 'provision-qt.py'):
        shutil.copyfile(ROOT / 'tools/release/docker' / name, context / name)
    shutil.copyfile(ROOT / 'dependencies.json', context / 'dependencies.json')
    shutil.copyfile(ROOT / 'engine/manifest.json', context / 'manifest.json')
    # Include the input pins in the tag so concurrent dependency versions use distinct images.
    import hashlib
    image = 'arbiter-linux-builder:' + hashlib.sha256(b''.join(
        (context / name).read_bytes() for name in ('Dockerfile', 'entrypoint.sh', 'provision-qt.py', 'dependencies.json', 'manifest.json')
    )).hexdigest()[:16]
    external_qt = os.environ.get('ARBITER_LINUX_QT_PREFIX')
    if external_qt:
        external_qt = Path(external_qt).resolve()
        if not (external_qt / 'bin/qtpaths').is_file():
            raise SystemExit('ARBITER_LINUX_QT_PREFIX must point to a Linux Qt kit containing bin/qtpaths')
        image += '-external-qt'
    bundle = ROOT / 'build-release-docker/source.bundle'
    run('git', 'bundle', 'create', bundle, 'HEAD', '--tags')
    output = ROOT / 'build-release-artifacts'; output.mkdir(exist_ok=True)
    jobs = str(args.jobs)
    run('docker', 'build', '--platform', 'linux/amd64', '--build-arg', 'ARBITER_BUILD_JOBS=' + jobs,
        '--build-arg', 'ARBITER_EXTERNAL_QT=' + str(bool(external_qt)).lower(), '-t', image, context)
    qt_args = []
    if external_qt:
        qt_args = ['--mount', f'type=bind,src={external_qt},dst=/opt/qt-kit,readonly',
                   '--env', 'ARBITER_QT_PREFIX=/opt/qt-kit']
    run('docker', 'run', '--rm', '--platform', 'linux/amd64', '--shm-size=4g',
        '--user', uid + ':' + gid,
        '--security-opt', 'seccomp=' + str(ROOT / 'tools/release/docker/seccomp-userns.json'),
        '--mount', f'type=bind,src={bundle},dst=/source.bundle,readonly',
        '--mount', f'type=bind,src={output},dst=/artifacts',
        '--mount', f'type=volume,src=arbiter-linux-build-cache-{uid},dst=/work',
        *qt_args, image, '--jobs', jobs)


if __name__ == '__main__': main()
