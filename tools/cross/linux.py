#!/usr/bin/env python3
"""Cross-build Linux x64 directly on macOS with native Clang/LLD. No containers."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import sys
import tarfile

import windows as common
import linux_sysroot
import linux_ffmpeg

ROOT, HERE, ENGINE, DEPS = common.ROOT, common.HERE, common.ENGINE, common.DEPS
PINS = linux_sysroot.PINS
run, atomic_json = common.run, common.atomic_json


def elf_x64(path):
    with path.open('rb') as stream:
        header = stream.read(20)
    if len(header) != 20 or header[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', header, 18)[0] != 62:
        raise RuntimeError(f'Expected Linux x64 ELF, got {path}')


def python(args):
    return args.work / 'python/bin/python'


def qt(args):
    return args.work / 'qt' / ENGINE['qt'] / 'gcc_64'


def identity(args):
    for tool, prefix in [('clang', args.llvm), ('ld.lld', args.lld)]:
        output = run(prefix / 'bin' / tool, '--version', capture=True)
        if PINS['llvm_version'] not in output:
            raise RuntimeError(f'{tool} must match pinned LLVM {PINS["llvm_version"]}')
    if run(args.host_qt / 'bin/qtpaths', '--query', 'QT_VERSION', capture=True).strip() != ENGINE['qt']:
        raise RuntimeError('Native Qt host tools must match the target Qt version')
    digest = hashlib.sha256()
    for path in [HERE / name for name in ('linux.py', 'linux_sysroot.py', 'linux-sysroot.json',
                 'linux-packages.lock.json', 'linux.patch', 'linux_ffmpeg.py', 'linux-ffmpeg.json',
                 'windows.py', 'qtwebengine.patch')
                 if (HERE / name).exists()] + [ROOT / 'cmake/toolchains/LinuxClang.cmake']:
        digest.update(path.name.encode() + b'\0' + path.read_bytes())
    return dict(engine_inputs_sha256=common.fingerprint(), cross_inputs_sha256=digest.hexdigest(),
                target='x86_64-unknown-linux-gnu', host='darwin', qt=ENGINE['qt'], llvm=PINS['llvm_version'],
                linux_ffmpeg=linux_ffmpeg.abi(args.ffmpeg))


def environment(args):
    env = os.environ.copy()
    root = args.work / 'sysroot'
    env.update(PKG_CONFIG_PATH='', PKG_CONFIG_SYSROOT_DIR=str(root),
        PKG_CONFIG_LIBDIR=os.pathsep.join(str(root / suffix) for suffix in (
            'usr/lib/x86_64-linux-gnu/pkgconfig', 'usr/lib/pkgconfig', 'usr/share/pkgconfig')),
        PATH=str(args.work / 'tools/bin') + os.pathsep + env['PATH'], NINJAFLAGS=f'-j{args.jobs}')
    if args.ffmpeg != 6:
        pc = root / linux_ffmpeg.prefix(args.ffmpeg).relative_to('/') / 'lib/pkgconfig'
        env['PKG_CONFIG_LIBDIR'] = str(pc) + os.pathsep + env['PKG_CONFIG_LIBDIR']
    # Do not inherit macOS SDK/compiler settings into target configure checks.
    for key in ('SDKROOT', 'MACOSX_DEPLOYMENT_TARGET', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS', 'CPATH',
                'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH'):
        env.pop(key, None)
    return env


def tool_facade(args):
    tools = args.work / 'tools'
    (tools / 'bin').mkdir(parents=True, exist_ok=True)
    for item in (args.llvm / 'bin').iterdir():
        link = tools / 'bin' / item.name
        if not link.exists() and not link.is_symlink():
            link.symlink_to(item)
    linker = tools / 'bin/ld.lld'
    if linker.is_symlink():
        linker.unlink()
    linker.symlink_to(args.lld / 'bin/ld.lld')
    # Chromium's ELF TOC helper invokes the GNU-style name even when the
    # configured compiler/binutils are LLVM. Keep that helper native too.
    reader = tools / 'bin/readelf'
    if reader.is_symlink():
        reader.unlink()
    elif reader.exists():
        raise RuntimeError(f'Refusing to replace non-symlink {reader}')
    reader.symlink_to(args.llvm / 'bin/llvm-readelf')
    for language in ('clang', 'clang++'):
        wrapper = tools / 'bin' / ('x86_64-linux-gnu-' + language)
        command = [args.llvm / 'bin' / language, '--target=x86_64-unknown-linux-gnu',
                   '--sysroot=' + str(args.work / 'sysroot'), '--gcc-toolchain=' + str(args.work / 'sysroot/usr'),
                   '-fuse-ld=' + str(args.lld / 'bin/ld.lld')]
        wrapper.write_text('#!/bin/sh\nexec ' + shlex.join(map(str, command)) + ' "$@"\n')
        wrapper.chmod(0o755)
    clang = tools / 'lib/clang/22'
    clang.mkdir(parents=True, exist_ok=True)
    for suffix in ('include', 'lib/darwin'):
        destination = clang / suffix
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            destination.symlink_to(args.llvm / 'lib/clang/22' / suffix)
    return tools


def flags(args):
    elf_x64(qt(args) / 'lib/libQt6Core.so.6')
    return ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        '-DARBITER_LINUX_FFMPEG_PREFIX=' + (str(linux_ffmpeg.prefix(args.ffmpeg)) if args.ffmpeg != 6 else ''),
        f'-DCMAKE_TOOLCHAIN_FILE={ROOT / "cmake/toolchains/LinuxClang.cmake"}',
        f'-DARBITER_LINUX_SYSROOT={args.work / "sysroot"}', f'-DARBITER_LINUX_TOOLS={args.work / "tools"}',
        f'-DARBITER_LINUX_LLVM={args.llvm}', f'-DARBITER_LINUX_QT={qt(args)}',
        f'-DCMAKE_PREFIX_PATH={qt(args)}', f'-DQT_HOST_PATH={args.host_qt}']


def bootstrap(args):
    py = python(args)
    if not py.exists():
        run('python3.12', '-m', 'venv', py.parent.parent)
    run(py, '-m', 'pip', 'install', f'python-debian=={PINS["python_debian"]}', 'html5lib==1.1',
        f'git+https://github.com/miurahr/aqtinstall.git@{common.PINS["aqt_commit"]}')


def prepare(args):
    bootstrap(args)
    run(python(args), HERE / 'linux_sysroot.py', 'prepare', '--work', args.work)
    if not (qt(args) / 'lib/cmake/Qt6Core/Qt6CoreConfig.cmake').exists():
        run(python(args), '-m', 'aqt', 'install-qt', 'linux', 'desktop', ENGINE['qt'], 'linux_gcc_64',
            '--outputdir', args.work / 'qt', '--modules', 'qtwebchannel', 'qtpositioning', 'qtshadertools', cwd=args.work)
    tool_facade(args)
    linux_ffmpeg.prepare(args)



def configure(args, source, build, options, env):
    stamp = build / '.arbiter-configure.json'
    expected = dict(identity=identity(args), source=str(source), flags=options)
    refresh = not stamp.exists() or json.loads(stamp.read_text()) != expected
    stamp.unlink(missing_ok=True)
    run('cmake', *(['--fresh'] if refresh else []), '-S', source, '-B', build, *options, env=env)
    atomic_json(stamp, expected)


def probe(args):
    tool_facade(args)
    build = args.work / 'probe'
    configure(args, HERE / 'linux-probe', build, flags(args), environment(args))
    run('cmake', '--build', build, '--parallel', args.jobs, env=environment(args))
    executable = build / 'linux-probe'
    elf_x64(executable)
    info = run(args.llvm / 'bin/llvm-readelf', '-d', executable, capture=True)
    for required in ('libQt6Core.so', 'libavformat.so', 'libnss3.so', 'libcrypto.so'):
        if required not in info:
            raise RuntimeError(f'Target probe is missing dependency {required}')
    atomic_json(args.work / 'probe.json', dict(identity=identity(args), status='compiled-not-runtime-tested'))
    print(f'Linux x64 Qt/FFmpeg/NSS/OpenSSL probe linked: {executable}')


def runtime(args):
    tools = tool_facade(args)
    archive = linux_sysroot.fetch(PINS['llvm_source_url'], args.work / 'downloads/llvm-project.src.tar.xz', PINS['llvm_source_sha256'])
    source_root = args.work / 'llvm-source'
    source = source_root / ('llvm-project-' + PINS['llvm_version'] + '.src')
    if not (source / 'compiler-rt/CMakeLists.txt').exists():
        with tarfile.open(archive) as bundle:
            members = (item for item in bundle if len(item.name.split('/')) > 1
                       and item.name.split('/')[1] in ('compiler-rt', 'cmake'))
            bundle.extractall(source_root, members=members, filter='data')
    build = args.work / 'compiler-rt'
    run('cmake', '-G', 'Ninja', '-S', source / 'compiler-rt', '-B', build,
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_SYSTEM_NAME=Linux', '-DCMAKE_SYSTEM_PROCESSOR=x86_64',
        '-DCMAKE_C_COMPILER_TARGET=x86_64-unknown-linux-gnu',
        '-DCMAKE_CXX_COMPILER_TARGET=x86_64-unknown-linux-gnu',
        f'-DCMAKE_SYSROOT={args.work / "sysroot"}',
        f'-DCMAKE_C_COMPILER={tools / "bin/x86_64-linux-gnu-clang"}',
        f'-DCMAKE_CXX_COMPILER={tools / "bin/x86_64-linux-gnu-clang++"}',
        f'-DCMAKE_ASM_COMPILER={tools / "bin/x86_64-linux-gnu-clang"}',
        f'-DCMAKE_AR={args.llvm / "bin/llvm-ar"}', f'-DCMAKE_RANLIB={args.llvm / "bin/llvm-ranlib"}',
        f'-DLLVM_CONFIG_PATH={args.llvm / "bin/llvm-config"}',
        '-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON', '-DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-unknown-linux-gnu',
        '-DCOMPILER_RT_BUILD_BUILTINS=ON', '-DCOMPILER_RT_BUILD_SANITIZERS=OFF',
        '-DCOMPILER_RT_BUILD_XRAY=OFF', '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
        '-DCOMPILER_RT_BUILD_PROFILE=OFF', '-DCOMPILER_RT_BUILD_MEMPROF=OFF', '-DCOMPILER_RT_INCLUDE_TESTS=OFF')
    run('cmake', '--build', build, '--parallel', args.jobs)
    library = next(build.rglob('libclang_rt.builtins-x86_64.a'))
    info = run(args.llvm / 'bin/llvm-readobj', '--file-headers', library, capture=True)
    if 'elf64-x86-64' not in info or 'Mach-O' in info:
        raise RuntimeError('Compiler runtime was not built for Linux x64')
    for suffix in ('lib/clang/22/lib/x86_64-unknown-linux-gnu/libclang_rt.builtins.a',
                   'lib/clang/22/lib/linux/libclang_rt.builtins-x86_64.a'):
        destination = tools / suffix
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(library, destination)


def source(args):
    path = args.work / 'source'
    seed = ROOT / 'build-engine-source'
    if not path.exists() and seed.exists():
        run('git', 'clone', '--shared', '--no-checkout', seed, path)
        run('git', 'remote', 'set-url', 'origin', ENGINE['webengine_repository'], cwd=path)
        run('git', 'checkout', '--detach', ENGINE['webengine_commit'], cwd=path)
    if path.exists() and seed.exists() and not (path / 'src/3rdparty/.git').exists():
        # A local clone can reuse a shallow object store; git --reference cannot.
        run('git', 'clone', '--shared', '--no-checkout', seed / 'src/3rdparty', path / 'src/3rdparty')
        run('git', 'remote', 'set-url', 'origin', 'https://github.com/qt/qtwebengine-chromium.git', cwd=path / 'src/3rdparty')
        run('git', 'checkout', '--detach', ENGINE['chromium_commit'], cwd=path / 'src/3rdparty')
    # Reuse the already tested host-tool compatibility layer; Linux-specific
    # changes are applied separately and never alter native/Windows SDK inputs.
    extra = HERE / 'linux.patch'
    if path.exists() and extra.exists():
        check = subprocess.run(['patch', '--dry-run', '--force', '--fuzz=0', '-R', '-p1', '-i', str(extra)],
                               cwd=path, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if check.returncode == 0:
            run('patch', '--force', '--fuzz=0', '-R', '-p1', '-i', extra, cwd=path)
    path = common.prepare_source(args)
    run('patch', '--dry-run', '--force', '--fuzz=0', '-p1', '-i', extra, cwd=path)
    run('patch', '--force', '--fuzz=0', '-p1', '-i', extra, cwd=path)
    return path


def engine(args):
    before = identity(args)
    code = source(args)
    tools = tool_facade(args)
    linux_ffmpeg.prepare(args)
    build, sdk = args.work / 'engine', sdk_path(args)
    (sdk / 'arbiter-engine-build.json').unlink(missing_ok=True)
    options = flags(args) + [f'-DCMAKE_INSTALL_PREFIX={sdk}', f'-DPython3_EXECUTABLE={python(args)}',
        f'-DARBITER_WINDOWS_CLANG_PREFIX={tools}', f'-DQWELibClang_BIN_PATH={args.llvm / "bin"}',
        f'-DLibclang_LIBRARY={args.llvm / "lib/libclang.dylib"}',
        f'-DGn_EXECUTABLE={args.host_qt / "libexec/gn"}',
        '-DQT_BUILD_EXAMPLES=OFF', '-DQT_BUILD_TESTS=OFF', '-DQT_SHOW_EXTRA_IDE_SOURCES=OFF',
        '-DFEATURE_qtwebengine_build=ON', '-DFEATURE_qtpdf_build=ON',
        # Qt otherwise treats a cross-compiled Unix target as embedded and
        # silently disables WebRTC and browser printing/PDF integration.
        '-DFEATURE_webengine_embedded_build=OFF', '-DFEATURE_webengine_webrtc=ON',
        '-DFEATURE_webengine_printing_and_pdf=ON',
        '-DFEATURE_webengine_system_ffmpeg=ON', '-DFEATURE_webengine_proprietary_codecs=ON',
        '-DFEATURE_webengine_geolocation=ON', '-DFEATURE_webengine_extensions=ON',
        '-DFEATURE_webengine_rust_build=OFF']
    env = environment(args)
    configure(args, code, build, options, env)
    common.require_enabled_modules(build)
    cache = (build / 'CMakeCache.txt').read_text()
    for feature in ('webengine_webrtc', 'webengine_printing_and_pdf'):
        if not re.search(rf'^QT_FEATURE_{feature}:[^=]+=ON$', cache, re.MULTILINE):
            raise RuntimeError(f'Required desktop feature is disabled: {feature}')
    if args.configure_only:
        return
    run('cmake', '--build', build, '--parallel', args.jobs, env=env)
    if identity(args) != before:
        raise RuntimeError('Inputs changed during the Linux engine build')
    common.verify_media(code, build)
    run('cmake', '--install', build, env=env)
    elf_x64(sdk / 'lib/libQt6WebEngineCore.so.6')
    elf_x64(sdk / 'lib/libQt6Pdf.so.6')
    needed = run(args.llvm / 'bin/llvm-readelf', '-d', sdk / 'lib/libQt6WebEngineCore.so.6', capture=True)
    libraries = {name: int(major) for name, major in re.findall(
        r'Shared library: \[(libavcodec|libavformat|libavutil)\.so\.(\d+)\]', needed)}
    if libraries != linux_ffmpeg.abi(args.ffmpeg)['libraries']:
        raise RuntimeError('The built engine linked a different FFmpeg ABI: ' + repr(libraries))
    shutil.copyfile(ROOT / 'engine/manifest.json', sdk / 'arbiter-engine-manifest.json')
    shutil.copyfile(ROOT / 'engine/LICENSE', sdk / 'arbiter-engine-LICENSE.txt')
    atomic_json(sdk / 'arbiter-cross-build.json', before)
    atomic_json(sdk / 'arbiter-engine-build.json', dict(inputs_sha256=common.fingerprint(),
        qt=ENGINE['qt'], platform='linux', architecture='x86_64', linux_ffmpeg=linux_ffmpeg.abi(args.ffmpeg)))


def sdk_path(args):
    return args.work / ('sdk' if args.ffmpeg == 6 else f'sdk-ffmpeg{args.ffmpeg}')


def app(args):
    sdk = sdk_path(args)
    receipt = common.verify_sdk(sdk)
    if receipt.get('platform') != 'linux' or receipt.get('architecture') != 'x86_64':
        raise RuntimeError('The engine SDK must be Linux x64')
    if json.loads((sdk / 'arbiter-cross-build.json').read_text()) != identity(args):
        raise RuntimeError('Cross inputs changed; rebuild the Linux SDK')
    env = environment(args)
    cargo = run('rustup', 'which', '--toolchain', DEPS['rust'], 'cargo', capture=True).strip()
    run('rustup', 'target', 'add', '--toolchain', DEPS['rust'], 'x86_64-unknown-linux-gnu')
    env.update(RUSTUP_TOOLCHAIN=DEPS['rust'],
        CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_LINKER=str(args.work / 'tools/bin/x86_64-linux-gnu-clang'),
        CC_x86_64_unknown_linux_gnu=str(args.work / 'tools/bin/x86_64-linux-gnu-clang'),
        AR_x86_64_unknown_linux_gnu=str(args.llvm / 'bin/llvm-ar'))
    env['PATH'] = str(Path(cargo).parent) + os.pathsep + env['PATH']
    build = args.work / ('app' if args.ffmpeg == 6 else f'app-ffmpeg{args.ffmpeg}')
    options = flags(args) + [f'-DARBITER_WEBENGINE_PREFIX={sdk}',
        '-DARBITER_PATCHED_WEBENGINE=ON', '-DARBITER_STRICT_ENGINE_IDENTITY=ON',
        '-DARBITER_BUNDLED_KEYCHAIN=ON',
        '-DARBITER_WEBENGINE_SDK_RUNTIME=OFF', '-DARBITER_RUST_TARGET=x86_64-unknown-linux-gnu',
        f'-DPython3_EXECUTABLE={python(args)}', f'-DARBITER_CARGO={cargo}']
    configure(args, ROOT, build, options, env)
    run('cmake', '--build', build, '--parallel', args.jobs, env=env)
    elf_x64(build / 'arbiter')
    print(f'Linux x64 application compiled: {build / "arbiter"}. Runtime validation/packaging remains separate.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('lock-sysroot', 'prepare', 'probe', 'runtime', 'engine', 'app', 'all'))
    parser.add_argument('--work', type=Path, default=ROOT / 'build-linux-cross')
    parser.add_argument('--host-qt', type=Path, required=True)
    parser.add_argument('--llvm', type=Path, required=True)
    parser.add_argument('--lld', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=16)
    parser.add_argument('--configure-only', action='store_true')
    parser.add_argument('--ffmpeg', type=int, choices=(6, 8), default=6,
                        help='Target system FFmpeg ABI (6 for Ubuntu 24.04, 8 for Tumbleweed); codecs stay external')
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('This entry point runs natively on macOS')
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    for key in ('work', 'host_qt', 'llvm', 'lld'):
        setattr(args, key, getattr(args, key).resolve())
    if any(any(c.isspace() for c in str(path)) for path in (ROOT, args.work, args.host_qt, args.llvm, args.lld)):
        parser.error('Chromium cross-build paths must not contain whitespace')
    args.work.mkdir(parents=True, exist_ok=True)
    try:
        with (args.work / '.cross-build.lock').open('w') as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise RuntimeError('Another Linux cross-build phase is using this work directory') from None
            if args.phase == 'lock-sysroot':
                bootstrap(args)
                run(python(args), HERE / 'linux_sysroot.py', 'lock', '--work', args.work)
            elif args.phase == 'all':
                for action in (prepare, probe, runtime, engine, app):
                    action(args)
            else:
                globals()[args.phase](args)
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Linux cross-build failed: {error}\n')


if __name__ == '__main__':
    main()
