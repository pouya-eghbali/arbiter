#!/usr/bin/env python3
"""Exercise setup on a disposable Windows runner, preserving unrelated files."""
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('setup', type=Path)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Run on a disposable Windows validation runner')
    setup = args.setup.resolve()
    with tempfile.TemporaryDirectory(prefix='arbiter-setup-test-') as temporary:
        root = Path(temporary)
        destination = root / 'application'
        # /D must be last and unquoted: NSIS consumes the remaining command line.
        command = f'"{setup}" /S /D={destination}'
        destination.mkdir()
        unrelated = destination / 'keep.txt'
        unrelated.write_text('Unrelated data')
        assert subprocess.run(command, timeout=180).returncode != 0, 'Setup accepted an unrelated folder'
        assert unrelated.read_text() == 'Unrelated data'
        unrelated.unlink()
        subprocess.run(command, check=True, timeout=300)
        assert (destination / 'bin/arbiter.exe').is_file()
        assert json.loads((destination / 'share/arbiter/release.json').read_text())['platform'] == 'windows'
        # Updating/removing may delete installer-owned files only.
        unrelated.write_text('Unrelated data')
        subprocess.run(command, check=True, timeout=300)
        assert unrelated.read_text() == 'Unrelated data'
        uninstaller = destination / 'uninstall.exe'
        subprocess.run(f'"{uninstaller}" /S _?={destination}', check=True, timeout=180)
        assert not (destination / 'bin/arbiter.exe').exists()
        assert unrelated.read_text() == 'Unrelated data'
    print('Windows setup, update, refusal, and uninstall checks passed')


if __name__ == '__main__':
    main()
