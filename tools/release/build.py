#!/usr/bin/env python3
"""Native CI entry point: exact SDK inputs, full tests, then relocatable packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'engine'))
from identity import fingerprint, verify_sdk


def run(*args, env=None):
    print('+', subprocess.list2cmdline([str(a) for a in args]), flush=True)
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT, env=env)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--jobs', type=int, default=24)
    p.add_argument('--package', action='store_true')
    p.add_argument('--unsigned', action='store_true')
    a = p.parse_args()
    if not 1 <= a.jobs <= 96: p.error('--jobs must be between 1 and 96')
    if a.package:
        from package import require_clean_source
        require_clean_source()
    preflight = [sys.executable, ROOT / 'tools/release/preflight.py']
    if a.package and not a.unsigned: preflight += ['--signing']
    run(*preflight)
    qt = Path(os.environ['ARBITER_QT_PREFIX']).resolve()
    qtpaths = qt / 'bin' / ('qtpaths.exe' if sys.platform == 'win32' else 'qtpaths')
    expected_qt = json.loads((ROOT / 'engine/manifest.json').read_text())['qt']
    actual_qt = subprocess.check_output([str(qtpaths), '--query', 'QT_VERSION'], text=True).strip()
    if actual_qt != expected_qt: raise RuntimeError(f'Builder needs Qt {expected_qt}, found {actual_qt}')
    pins = json.loads((ROOT / 'dependencies.json').read_text())
    cargo = Path(subprocess.check_output(['rustup', 'which', '--toolchain', pins['rust'], 'cargo'], text=True).strip())
    os.environ['PATH'] = str(cargo.parent) + os.pathsep + os.environ.get('PATH', '')
    os.environ['RUSTUP_TOOLCHAIN'] = pins['rust']
    rust = subprocess.check_output(['rustc', '--version'], text=True).split()[1]
    if rust != pins['rust']: raise RuntimeError(f'Expected Rust {pins["rust"]}, found {rust}; select the pinned rustup toolchain')
    import importlib.metadata
    if importlib.metadata.version('html5lib') != pins['html5lib']: raise RuntimeError('Install the pinned html5lib version')
    run(sys.executable, '-m', 'unittest', 'discover', '-s', 'engine', '-p', 'test_*.py')
    run(sys.executable, '-m', 'unittest', 'discover', '-s', 'tools/release', '-p', 'test_*.py')
    run('cargo', 'test', '--manifest-path', ROOT / 'rust/adblock/Cargo.toml', '--locked',
        '--release', '--jobs', a.jobs, '--target-dir', ROOT / 'build-release-rust-tests')
    cache = Path(os.environ.get('ARBITER_ENGINE_CACHE', str(ROOT / 'build-release-engine')))
    kit_path_key = hashlib.sha256(str(qt).encode()).hexdigest()[:12]
    work = (cache / (fingerprint() + '-' + sys.platform + '-' + platform.machine() + '-' + kit_path_key)).resolve()
    sdk = work / 'sdk'
    if (sdk / 'arbiter-engine-build.json').exists():
        verify_sdk(sdk)
    else:
        run(sys.executable, ROOT / 'engine/build.py', '--qt-prefix', qt, '--work', work, '--jobs', a.jobs)
    verify_sdk(sdk)
    output = ROOT / 'build-release-artifacts'
    output.mkdir(exist_ok=True)
    definitions = ['-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PREFIX_PATH=' + str(qt),
                   '-DARBITER_PATCHED_WEBENGINE=ON', '-DARBITER_WEBENGINE_PREFIX=' + str(sdk),
                   '-DARBITER_STRICT_ENGINE_IDENTITY=ON', '-DARBITER_BUNDLED_KEYCHAIN=ON']
    if os.environ.get('ARBITER_OPENSSL_ROOT'):
        definitions += ['-DOPENSSL_ROOT_DIR=' + os.environ['ARBITER_OPENSSL_ROOT']]
    if os.environ.get('ARBITER_CMAKE_TOOLCHAIN'):
        definitions += ['-DCMAKE_TOOLCHAIN_FILE=' + os.environ['ARBITER_CMAKE_TOOLCHAIN']]
    if sys.platform == 'darwin': definitions += ['-DCMAKE_OSX_ARCHITECTURES=' + platform.machine(), '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0']
    tests = ROOT / 'build-release-tests'
    run('cmake', '-S', ROOT / 'tests', '-B', tests, '-G', 'Ninja', *definitions, '-DARBITER_WEBENGINE_SDK_RUNTIME=ON')
    run('cmake', '--build', tests, '--parallel', a.jobs)
    environment = dict(os.environ, ARBITER_TEST_KEYCHAIN='1')
    if sys.platform.startswith('linux'):
        run('xvfb-run', '-a', 'dbus-run-session', '--', 'bash', '-e', '-c',
            'export GNOME_KEYRING_CONTROL; GNOME_KEYRING_CONTROL=$(mktemp -d); '
            'printf "arbiter-ci-keyring" | gnome-keyring-daemon --unlock --components=secrets --control-directory="$GNOME_KEYRING_CONTROL"; '
            'ctest --test-dir build-release-tests --output-on-failure --timeout 300', env=environment)
    else:
        run('ctest', '--test-dir', tests, '--output-on-failure', '--timeout', '300', env=environment)
    app = ROOT / 'build-release-app'
    run('cmake', '-S', ROOT, '-B', app, '-G', 'Ninja', *definitions,
        '-DARBITER_WEBENGINE_SDK_RUNTIME=OFF', '-DARBITER_MACOS_SIGNING_IDENTITY=-')
    run('cmake', '--build', app, '--parallel', a.jobs)
    run('cmake', '--build', app, '--target', 'arbiter_qmllint')
    run(sys.executable, ROOT / 'tools/translations/validate.py')
    if a.package:
        args = [sys.executable, ROOT / 'tools/release/package.py', '--build', app, '--sdk', sdk, '--qt-prefix', qt, '--output', output]
        if a.unsigned: args += ['--unsigned']
        run(*args)
    else:
        archive = output / f'arbiter-webengine-{sys.platform}-{platform.machine()}-{fingerprint()}.tar.gz'
        with tarfile.open(archive, 'w:gz') as tar: tar.add(sdk, arcname='sdk')
        print(archive)


if __name__ == '__main__': main()
