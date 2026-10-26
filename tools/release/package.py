#!/usr/bin/env python3
"""Stage and package a native release; never use SDK runtime path overrides."""
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
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'engine'))
from identity import verify_sdk


def run(*args, capture=False):
    print('+', subprocess.list2cmdline([str(a) for a in args]), flush=True)
    return subprocess.run([str(a) for a in args], check=True, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout


def copy(source, destination):
    if source.is_dir():
        shutil.copytree(source, destination, symlinks=True, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('*.dSYM', '*.pdb', '*.debug'))
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.is_symlink():
            destination.unlink()
        shutil.copy2(source, destination, follow_symlinks=False)


def require_clean_source():
    status = run('git', '-C', ROOT, 'status', '--porcelain', '--untracked-files=all',
                 '--', '.', ':(exclude)output/**', capture=True)
    if status.strip():
        raise RuntimeError('Commit source changes before packaging; release metadata must identify the exact source')


def audit(root):
    # Linux codecs remain system dependencies; no Widevine is redistributed.
    forbidden = ('widevine', 'libavcodec', 'libavformat', 'libavutil', 'libswresample',
                 'libswscale', 'openh264')
    for path in root.rglob('*'):
        if path.is_file() and any(name in path.name.lower() for name in forbidden):
            raise RuntimeError(f'Forbidden bundled codec/CDM: {path}')
    for name in ('QtWebEngineProcess', 'qtwebengine_resources.pak', 'en-US.pak'):
        if not any(p.name in (name, name + '.exe') for p in root.rglob('*')):
            raise RuntimeError(f'Package is missing {name}')


def engine_symbol(root):
    # Deployment tools must never substitute the stock Qt WebEngine library.
    if sys.platform == 'win32':
        libraries = list(root.rglob('Qt6WebEngineCore.dll'))
        command = ('dumpbin', '/exports')
    elif sys.platform == 'darwin':
        libraries = [p for p in root.rglob('QtWebEngineCore') if p.is_file() and not p.is_symlink()
                     and p.parent.parent.name == 'Versions']
        command = ('nm', '-g')
    else:
        libraries = [p for p in root.rglob('libQt6WebEngineCore.so.*') if p.is_file() and not p.is_symlink()]
        command = ('nm', '-D')
    if not libraries:
        raise RuntimeError('Packaged WebEngine library is missing')
    for library in libraries:
        output = run(*command, library, capture=True)
        if 'qWebEngineArbiterExtensionApiAbi' not in output:
            raise RuntimeError(f'Stock WebEngine was deployed: {library}')


def machos(root):
    magic = {bytes.fromhex(x) for x in ('feedface', 'feedfacf', 'cefaedfe', 'cffaedfe', 'cafebabe', 'bebafeca', 'cafebabf', 'bfbafeca')}
    result = []
    for path in root.rglob('*'):
        if path.is_file() and not path.is_symlink() and not any(part.endswith('.dSYM') for part in path.parts):
            with path.open('rb') as stream:
                if stream.read(4) in magic: result.append(path)
    return result


def macos_engine(app, sdk):
    # macdeployqt resolves @rpath frameworks using the base kit first, even with
    # -libpath. Replace the engine, helper, resources and QML as one unit AFTER it.
    for module in ('QtWebEngineCore', 'QtWebEngineQuick', 'QtWebEngineWidgets'):
        source = sdk / 'lib' / (module + '.framework')
        destination = app / 'Contents/Frameworks' / source.name
        if source.exists() and (destination.exists() or module != 'QtWebEngineWidgets'):
            if destination.exists(): shutil.rmtree(destination)
            shutil.copytree(source, destination, symlinks=True,
                            ignore=shutil.ignore_patterns('Headers', '*.dSYM', '*.pdb', '*.debug'))
            versions = destination / 'Versions'
            current = (versions / 'Current').resolve()
            if current.parent != versions.resolve():
                raise RuntimeError(f'Unexpected framework version layout: {destination}')
            # Development SDK installs can retain empty Resources directories
            # alongside A/Current. Apple treats these as unsigned extra versions.
            for extra in versions.iterdir():
                if extra.name not in ('Current', current.name):
                    if extra.is_dir() and not extra.is_symlink(): shutil.rmtree(extra)
                    else: extra.unlink()
    module = app / 'Contents/Resources/qml/QtWebEngine'
    if module.exists(): shutil.rmtree(module)
    copy(sdk / 'qml/QtWebEngine', module)
    for plugin in (sdk / 'qml/QtWebEngine').rglob('*.dylib'):
        destination = app / 'Contents/PlugIns/quick' / plugin.name
        if destination.exists(): copy(plugin, destination)
    # Only SQLite is used; optional Qt SQL drivers require unrelated vendor
    # databases and otherwise introduce missing build-machine library paths.
    for driver in (app / 'Contents/PlugIns/sqldrivers').glob('*'):
        if driver.name != 'libqsqlite.dylib':
            if driver.is_dir(): shutil.rmtree(driver)
            else: driver.unlink()


def sign_macos(app, identity, timestamp):
    binaries = machos(app)
    # Drop absolute build-kit RPATHs; deployed relative paths are retained.
    for binary in binaries:
        architectures = run('lipo', '-archs', binary, capture=True).split()
        if len(architectures) > 1:
            architecture = platform.machine()
            if architecture not in architectures:
                raise RuntimeError(f'Package architecture {architecture} is missing from {binary}')
            temporary = binary.with_name(binary.name + '.thin')
            run('lipo', binary, '-thin', architecture, '-output', temporary)
            temporary.chmod(binary.stat().st_mode)
            temporary.replace(binary)
        lines = run('otool', '-l', binary, capture=True).splitlines()
        rpaths, all_rpaths = set(), set()
        for index, line in enumerate(lines):
            if line.strip() == 'cmd LC_RPATH':
                path = lines[index + 2].strip().removeprefix('path ').rsplit(' (offset ', 1)[0]
                all_rpaths.add(path)
                if path.startswith('/') and not path.startswith(('/System/', '/usr/lib/')): rpaths.add(path)
        for path in sorted(rpaths): run('install_name_tool', '-delete_rpath', path, binary)
        bundle_libraries = '@loader_path/' + os.path.relpath(app / 'Contents/Frameworks', binary.parent)
        if bundle_libraries not in all_rpaths:
            run('install_name_tool', '-add_rpath', bundle_libraries, binary)
        linked = run('otool', '-L', binary, capture=True).splitlines()[1:]
        for line in linked:
            if ' (compatibility version ' not in line: continue
            dependency = line.strip().split(' (compatibility', 1)[0]
            if dependency.startswith('/') and not dependency.startswith(('/System/', '/usr/lib/')):
                raise RuntimeError(f'External build dependency in deployed bundle: {binary}: {dependency}')
    common = ['--force', '--sign', identity]
    if timestamp: common += ['--timestamp', '--options', 'runtime']
    # Nested code first; never use --deep to sign a bundle.
    for binary in sorted(binaries, key=lambda p: len(p.parts), reverse=True):
        args = common[:]
        if binary.name == 'QtWebEngineProcess':
            entitlements = list(binary.parents[1].rglob('QtWebEngineProcess.entitlements'))
            if not entitlements: raise RuntimeError('Qt helper entitlements are missing')
            args += ['--entitlements', str(entitlements[0])]
        run('codesign', *args, binary)
    bundles = [p for p in app.rglob('*') if p.is_dir() and not p.is_symlink() and p.suffix in ('.app', '.framework')]
    for bundle in sorted(bundles, key=lambda p: len(p.parts), reverse=True):
        args = common[:]
        if bundle.name == 'QtWebEngineProcess.app':
            entitlements = list(bundle.rglob('QtWebEngineProcess.entitlements'))
            args += ['--entitlements', str(entitlements[0])]
        run('codesign', *args, bundle)
    run('codesign', *common, '--entitlements', app / 'Contents/Resources/Arbiter.entitlements', app)
    run('codesign', '--verify', '--deep', '--strict', app)


def smoke(stage, sdk, qt):
    env = {k: v for k, v in os.environ.items() if not k.startswith(('QT_', 'QTWEBENGINE_', 'QML', 'DYLD_')) and k not in ('LD_LIBRARY_PATH', 'LD_PRELOAD')}
    # In particular, Windows must not find missing DLLs in the installed kit.
    env['PATH'] = os.pathsep.join(part for part in env.get('PATH', '').split(os.pathsep)
                                  if part and not any(Path(part).resolve().is_relative_to(prefix) for prefix in (sdk, qt)))
    env['QT_QUICK_BACKEND'] = 'software'
    with tempfile.TemporaryDirectory(prefix='arbiter-release-smoke-') as temporary:
        for key in ('XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME', 'QML_DISK_CACHE_PATH'):
            env[key] = temporary
        if sys.platform == 'darwin': executable = stage / 'arbiter.app/Contents/MacOS/arbiter'
        elif sys.platform == 'win32': executable = stage / 'bin/arbiter.exe'
        else: executable = stage / 'arbiter'
        args = [str(executable), '--release-smoke-test']
        if sys.platform.startswith('linux'): args = ['xvfb-run', '-a', *args]
        subprocess.run(args, env=env, check=True, timeout=60)


def linux(stage, sdk, qt):
    # Match the distro baseline used by the builder. System FFmpeg, glibc,
    # desktop/graphics drivers, Secret Service and fprintd stay external.
    for prefix in (qt, sdk):
        for directory in ('qml', 'plugins'):
            if (prefix / directory).exists():
                if prefix == sdk and directory == 'qml' and (stage / 'qml/QtWebEngine').exists():
                    shutil.rmtree(stage / 'qml/QtWebEngine')
                copy(prefix / directory, stage / directory)
        for libdir in (prefix / 'lib', prefix / 'lib64'):
            if libdir.exists():
                for library in libdir.glob('libQt6*.so*'):
                    if prefix == qt and library.name.startswith('libQt6WebEngine'):
                        continue
                    copy(library, stage / 'lib' / library.name)
    for helper in sdk.rglob('QtWebEngineProcess'):
        if helper.is_file(): copy(helper, stage / 'libexec/QtWebEngineProcess'); break
    for resource in sdk.rglob('*'):
        if resource.is_file() and resource.suffix in ('.pak', '.dat', '.bin'):
            target = 'translations/qtwebengine_locales' if 'qtwebengine_locales' in resource.parts else 'resources'
            copy(resource, stage / target / resource.name)
    (stage / 'bin/qt.conf').write_text('[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\nLibraryExecutables=libexec\nData=.\nTranslations=translations\n')
    launcher = stage / 'arbiter'
    launcher.write_text('#!/bin/sh\nset -eu\nbase=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexport LD_LIBRARY_PATH="$base/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexec "$base/bin/arbiter" "$@"\n')
    launcher.chmod(0o755)
    for path in stage.rglob('*'):
        if path.is_file() and not path.is_symlink():
            with path.open('rb') as stream: elf = stream.read(4) == b'\x7fELF'
            if elf:
                relative = os.path.relpath(stage / 'lib', path.parent)
                run('patchelf', '--set-rpath', '$ORIGIN/' + relative, path)


def windows(stage, sdk, qt):
    executable = stage / 'bin/arbiter.exe'
    run(qt / 'bin/windeployqt.exe', '--release', '--qmldir', ROOT / 'qml',
        '--qmlimport', sdk / 'qml', '--dir', stage / 'bin', executable)
    # windeployqt queries the base kit; overwrite ALL engine pieces together.
    for library in ('Qt6WebEngineCore.dll', 'Qt6WebEngineQuick.dll', 'Qt6WebEngineWidgets.dll', 'QtWebEngineProcess.exe'):
        copy(sdk / 'bin' / library, stage / 'bin' / library)
    module = stage / 'bin/qml/QtWebEngine'
    if module.exists(): shutil.rmtree(module)
    copy(sdk / 'qml/QtWebEngine', module)
    copy(sdk / 'resources', stage / 'bin/resources')
    copy(sdk / 'translations/qtwebengine_locales', stage / 'bin/translations/qtwebengine_locales')
    openssl = Path(os.environ['ARBITER_OPENSSL_BIN'])
    libraries = list(openssl.glob('libcrypto-3*.dll'))
    if not libraries: raise RuntimeError('ARBITER_OPENSSL_BIN has no OpenSSL 3 runtime DLL')
    for library in libraries: copy(library, stage / 'bin' / library.name)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--sdk', type=Path, required=True)
    p.add_argument('--qt-prefix', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--unsigned', action='store_true', help='Explicitly create a testing-only, unsigned draft')
    a = p.parse_args()
    sdk, qt, build, output = (x.resolve() for x in (a.sdk, a.qt_prefix, a.build, a.output))
    require_clean_source()
    verify_sdk(sdk)
    if sys.platform == 'darwin' and not a.unsigned:
        if not os.environ.get('ARBITER_MACOS_SIGNING_IDENTITY') or not os.environ.get('ARBITER_NOTARY_PROFILE'):
            p.error('Signed macOS releases require ARBITER_MACOS_SIGNING_IDENTITY and ARBITER_NOTARY_PROFILE')
    if sys.platform == 'win32' and not a.unsigned and not os.environ.get('ARBITER_WINDOWS_CERT_SHA1'):
        p.error('Signed Windows releases require ARBITER_WINDOWS_CERT_SHA1')
    cache = (build / 'CMakeCache.txt').read_text()
    for required in ('ARBITER_WEBENGINE_SDK_RUNTIME:BOOL=OFF', 'ARBITER_STRICT_ENGINE_IDENTITY:BOOL=ON', 'ARBITER_BUNDLED_KEYCHAIN:BOOL=ON'):
        if required not in cache: raise RuntimeError(f'Release build must set {required}')
    version = (ROOT / 'VERSION.txt').read_text().strip()
    arch = {'aarch64': 'arm64', 'AMD64': 'x86_64'}.get(platform.machine(), platform.machine())
    os_name = {'darwin': 'macos', 'win32': 'windows'}.get(sys.platform, 'linux')
    name = f'arbiter-{version}-{os_name}-{arch}' + ('-unsigned' if a.unsigned and os_name != 'linux' else '')
    output.mkdir(parents=True, exist_ok=True)
    stage = output / name
    if stage.exists(): raise RuntimeError(f'Refusing to overwrite staged package: {stage}')
    run('cmake', '--install', build, '--prefix', stage, '--config', 'Release')
    resources = stage / 'arbiter.app/Contents/Resources' if sys.platform == 'darwin' else stage / 'share/arbiter'
    copy(ROOT / 'licenses', resources / 'licenses')
    copy(ROOT / 'LICENSE', resources / 'licenses/LICENSE')
    copy(sdk / 'arbiter-engine-manifest.json', resources / 'arbiter-engine-manifest.json')
    copy(sdk / 'arbiter-engine-build.json', resources / 'arbiter-engine-build.json')
    if (sdk / 'sbom').exists(): copy(sdk / 'sbom', resources / 'sbom')
    metadata = {'version': version, 'commit': run('git', '-C', ROOT, 'rev-parse', 'HEAD', capture=True).strip(),
                'platform': os_name, 'architecture': arch, 'signed': not a.unsigned and os_name != 'linux',
                'engine': json.loads((sdk / 'arbiter-engine-build.json').read_text()),
                'dependencies': json.loads((ROOT / 'dependencies.json').read_text())}
    (resources / 'release.json').write_text(json.dumps(metadata, indent=2) + '\n')
    if sys.platform == 'darwin':
        app = stage / 'arbiter.app'
        copy(ROOT / 'tools/release/macos.entitlements', resources / 'Arbiter.entitlements')
        options = ['-no-strip', '-qmldir=' + str(ROOT / 'qml'), '-qmlimport=' + str(sdk / 'qml'), '-libpath=' + str(sdk / 'lib')]
        options.append('-no-codesign')
        run(qt / 'bin/macdeployqt', app, *options)
        macos_engine(app, sdk)
        audit(stage); engine_symbol(stage)
        sign_macos(app, '-' if a.unsigned else os.environ['ARBITER_MACOS_SIGNING_IDENTITY'], not a.unsigned)
        smoke(stage, sdk, qt)
        archive = output / (name + '.zip')
        run('ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', app, archive)
        if not a.unsigned:
            run('xcrun', 'notarytool', 'submit', archive, '--keychain-profile', os.environ['ARBITER_NOTARY_PROFILE'], '--wait')
            run('xcrun', 'stapler', 'staple', app)
            run('codesign', '--verify', '--deep', '--strict', app)
            archive.unlink()
            run('ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', app, archive)
    elif sys.platform == 'win32':
        windows(stage, sdk, qt); audit(stage); engine_symbol(stage)
        smoke(stage, sdk, qt)
        if not a.unsigned:
            # Sign Arbiter; dependency vendors' existing signatures are preserved.
            run('signtool', 'sign', '/sha1', os.environ['ARBITER_WINDOWS_CERT_SHA1'], '/fd', 'SHA256',
                '/tr', os.environ.get('ARBITER_TIMESTAMP_URL', 'http://timestamp.digicert.com'), '/td', 'SHA256', stage / 'bin/arbiter.exe')
            run('signtool', 'verify', '/pa', stage / 'bin/arbiter.exe')
        archive = Path(shutil.make_archive(str(output / name), 'zip', root_dir=stage.parent, base_dir=stage.name))
    else:
        linux(stage, sdk, qt); audit(stage); engine_symbol(stage)
        smoke(stage, sdk, qt)
        archive = output / (name + '.tar.xz')
        with tarfile.open(archive, 'w:xz') as tar: tar.add(stage, arcname=stage.name)
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    (output / (archive.name + '.sha256')).write_text(digest + '  ' + archive.name + '\n')
    print(archive)


if __name__ == '__main__': main()
