"""Compile-only FFmpeg ABI SDKs. Their shared libraries are never packaged."""
import json
from pathlib import Path
import subprocess
import tarfile

import linux_sysroot

HERE = Path(__file__).resolve().parent
PINS = json.loads((HERE / 'linux-ffmpeg.json').read_text())


def abi(major):
    if major == 6:
        return dict(major=6, libraries=dict(libavcodec=60, libavformat=60, libavutil=58))
    return dict(major=major, libraries=PINS[str(major)]['libraries'])


def prefix(major):
    return Path('/usr/local') / f'arbiter-ffmpeg{major}'


def prepare(args):
    if args.ffmpeg == 6:
        return
    pin = PINS[str(args.ffmpeg)]
    target = args.work / 'sysroot' / prefix(args.ffmpeg).relative_to('/')
    stamp = target / 'arbiter-link-sdk.json'
    expected = dict(pin=pin, purpose='link-only-no-codecs', compiler=str(args.llvm))
    if stamp.is_file() and json.loads(stamp.read_text()) == expected:
        return
    archive = linux_sysroot.fetch(pin['url'], args.work / 'downloads' / f'ffmpeg-{pin["version"]}.tar.xz', pin['sha256'])
    source_root = args.work / 'ffmpeg-link-source'
    source = source_root / f'ffmpeg-{pin["version"]}'
    if not source.is_dir():
        with tarfile.open(archive) as bundle:
            bundle.extractall(source_root, filter='data')
    build = args.work / f'ffmpeg{args.ffmpeg}-link-build'
    build.mkdir(exist_ok=True)
    compiler = args.work / 'tools/bin'
    command = [str(source / 'configure'), '--prefix=' + str(prefix(args.ffmpeg)),
        '--target-os=linux', '--arch=x86_64', '--enable-cross-compile',
        '--cc=' + str(compiler / 'x86_64-linux-gnu-clang'),
        '--cxx=' + str(compiler / 'x86_64-linux-gnu-clang++'),
        *['--' + name + '=' + str(args.llvm / 'bin' / ('llvm-' + name))
          for name in ('ar', 'nm', 'ranlib', 'strip')],
        '--disable-everything', '--disable-programs', '--disable-doc',
        '--disable-autodetect', '--disable-x86asm', '--disable-network',
        '--disable-static', '--enable-shared']
    subprocess.run(command, cwd=build, check=True)
    subprocess.run(['make', '-j' + str(args.jobs)], cwd=build, check=True)
    subprocess.run(['make', 'DESTDIR=' + str(args.work / 'sysroot'), 'install'], cwd=build, check=True)
    stamp.write_text(json.dumps(expected, indent=2) + '\n')
