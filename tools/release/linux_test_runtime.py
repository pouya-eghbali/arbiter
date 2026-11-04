#!/usr/bin/env python3
"""Provision FFmpeg 8 on the disposable Ubuntu validation runner, never in an app bundle."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/cross'))
from linux_sysroot import fetch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if not sys.platform.startswith('linux') or os.environ.get('GITHUB_ACTIONS') != 'true':
        parser.error('This system installation is only for a disposable Linux GitHub Actions runner')
    if not 1 <= args.jobs <= 96:
        parser.error('--jobs must be between 1 and 96')
    pin = json.loads((ROOT / 'tools/cross/linux-ffmpeg.json').read_text())['8']
    work = ROOT / 'build-cross-validation/test-ffmpeg8'
    work.mkdir(parents=True, exist_ok=True)
    archive = fetch(pin['url'], work / 'source.tar.xz', pin['sha256'])
    with tarfile.open(archive) as bundle:
        bundle.extractall(work, filter='data')
    source = work / ('ffmpeg-' + pin['version'])
    # Test codecs live on the runner only. package.audit explicitly rejects
    # copying libav* into release artifacts, including this runtime.
    subprocess.run([str(source / 'configure'), '--prefix=/usr/local',
        '--disable-programs', '--disable-doc', '--disable-debug', '--disable-autodetect',
        '--disable-x86asm', '--disable-static', '--enable-shared'], cwd=source, check=True)
    subprocess.run(['make', '-j' + str(args.jobs)], cwd=source, check=True)
    subprocess.run(['sudo', 'make', 'install'], cwd=source, check=True)
    subprocess.run(['sudo', 'ldconfig'], check=True)


if __name__ == '__main__':
    main()
