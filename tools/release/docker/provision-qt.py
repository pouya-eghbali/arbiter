#!/usr/bin/env python3
"""Provision base Qt plus QtPdf from the separately pinned WebEngine sources."""
import json
from pathlib import Path
import shutil
import subprocess


def run(*args, cwd=None):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)


manifest = json.loads(Path('/opt/arbiter-pins/manifest.json').read_text())
run('aqt', 'install-qt', 'linux', 'desktop', manifest['qt'], 'linux_gcc_64',
    '-O', '/opt/qt', '-m', 'qtwebchannel', 'qtpositioning')
qt = Path('/opt/qt') / manifest['qt'] / 'gcc_64'
# QtPdf shares WebEngine's independent version series starting with Qt 6.12.
# Build it against the base kit instead of assuming an old qtwebengine archive exists.
source = Path('/tmp/arbiter-qtpdf-source')
run('git', 'init', source)
run('git', 'remote', 'add', 'origin', manifest['webengine_repository'], cwd=source)
run('git', 'fetch', '--depth=1', 'origin', manifest['webengine_commit'], cwd=source)
run('git', 'checkout', '--detach', 'FETCH_HEAD', cwd=source)
run('git', '-c', 'url.https://github.com/.insteadOf=git@github.com:',
    'submodule', 'update', '--init', '--depth=1', 'src/3rdparty', cwd=source)
actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source / 'src/3rdparty', text=True).strip()
if actual != manifest['chromium_commit']:
    raise RuntimeError('QtPdf Chromium source differs from pinned manifest')
build = Path('/tmp/arbiter-qtpdf-build')
run('cmake', '-G', 'Ninja', '-S', source, '-B', build,
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PREFIX_PATH=' + str(qt),
    '-DCMAKE_INSTALL_PREFIX=' + str(qt), '-DFEATURE_qtwebengine_build=OFF',
    '-DFEATURE_qtpdf_build=ON', '-DQT_BUILD_EXAMPLES=OFF', '-DQT_BUILD_TESTS=OFF')
import os
jobs = os.environ.get('ARBITER_BUILD_JOBS', '24')
os.environ['NINJAFLAGS'] = '-j' + jobs
run('cmake', '--build', build, '--parallel', jobs)
run('cmake', '--install', build)
# Retain generated notices/build metadata beside the kit for release review.
for sbom in build.rglob('*.spdx.json'):
    target = qt / 'sbom' / sbom.name
    target.parent.mkdir(exist_ok=True)
    shutil.copyfile(sbom, target)
shutil.rmtree(source)
shutil.rmtree(build)
