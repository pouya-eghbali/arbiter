#!/usr/bin/env python3
"""Regenerate embedded Rust dependency notices from the locked Cargo sources."""
import argparse
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--cargo', default='cargo')
args = parser.parse_args()
env = dict(os.environ)
env.setdefault('CARGO_HOME', str(root / 'build-rust-cargo'))
cargo_path = Path(args.cargo)
if cargo_path.is_absolute():
    env['PATH'] = str(cargo_path.parent) + os.pathsep + env.get('PATH', '')
metadata = json.loads(subprocess.check_output([
    args.cargo, 'metadata', '--locked', '--offline', '--format-version', '1',
    '--manifest-path', str(root / 'rust/adblock/Cargo.toml')], env=env))
notices = ['Rust dependencies of Arbiter ad blocking\nGenerated from rust/adblock/Cargo.lock.\nUpstream sources are available from the listed crate registry/repositories.\n']
for package in sorted(metadata['packages'], key=lambda item: item['name']):
    if package['name'] == 'arbiter-adblock':
        continue
    directory = Path(package['manifest_path']).parent
    files = sorted({file for pattern in ('LICENSE*', 'LICENCE*', 'COPYING*') for file in directory.glob(pattern) if file.is_file()})
    if package.get('license_file'):
        files = sorted(set(files + [directory / package['license_file']]))
    supplement = root / 'licenses/adblock-extra' / (package['name'] + '.txt')
    if not files and supplement.exists():
        files = [supplement]
    if not files:
        raise SystemExit(f"Inspect missing license text for {package['name']} at {directory}")
    notices.append(f"\n{'=' * 72}\n{package['name']} {package['version']}\nLicense: {package.get('license')}\nRepository: {package.get('repository')}\nSource: https://crates.io/crates/{package['name']}/{package['version']}\n")
    for file in files:
        notices.append(f'\n--- {file.name} ---\n{file.read_text(errors="replace")}\n')
(root / 'licenses/AdBlockDependencies.txt').write_text(''.join(notices))
