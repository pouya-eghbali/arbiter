#!/usr/bin/env python3
"""Read-only checks before a native release build."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def windows_desktop_available():
    """GUI tests cannot expose windows on a service/background window station."""
    import ctypes
    from ctypes import wintypes

    class UserObjectFlags(ctypes.Structure):
        _fields_ = [('inherit', wintypes.BOOL), ('reserved', wintypes.BOOL),
                    ('flags', wintypes.DWORD)]

    user32 = ctypes.WinDLL('user32', use_last_error=True)
    user32.GetProcessWindowStation.restype = wintypes.HANDLE
    user32.GetUserObjectInformationW.argtypes = [wintypes.HANDLE, ctypes.c_int,
        wintypes.LPVOID, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    flags = UserObjectFlags()
    needed = wintypes.DWORD()
    return bool(user32.GetUserObjectInformationW(user32.GetProcessWindowStation(),
        1, ctypes.byref(flags), ctypes.sizeof(flags), ctypes.byref(needed))
        and flags.flags & 1)  # UOI_FLAGS, WSF_VISIBLE


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--signing', action='store_true', help='Also require the platform signing configuration')
    a = p.parse_args()
    failures = []
    def check(ok, message):
        print(('OK   ' if ok else 'FAIL ') + message)
        if not ok: failures.append(message)
    def tool(name): check(shutil.which(name) is not None, name + ' is on PATH')
    pins = json.loads((ROOT / 'dependencies.json').read_text())
    check(f'{sys.version_info.major}.{sys.version_info.minor}' == pins['python'], 'Python matches dependencies.json: ' + pins['python'])
    arch = platform.machine().lower()
    expected = 'arm64' if sys.platform == 'darwin' else 'x86_64'
    check({'amd64': 'x86_64', 'aarch64': 'arm64'}.get(arch, arch) == expected,
          'Native release architecture: ' + expected)
    for name in ('git', 'cmake', 'ninja', 'node'): tool(name)
    # Rust and html5lib are installed from dependencies.json by the workflow.
    check(shutil.which('rustup') is not None, 'rustup is available for the pinned Rust toolchain')
    qt = os.environ.get('ARBITER_QT_PREFIX')
    if qt:
        qtpaths = Path(qt) / 'bin' / ('qtpaths.exe' if sys.platform == 'win32' else 'qtpaths')
        expected_qt = json.loads((ROOT / 'engine/manifest.json').read_text())['qt']
        try: actual = subprocess.check_output([str(qtpaths), '--query', 'QT_VERSION'], text=True).strip()
        except (OSError, subprocess.SubprocessError): actual = 'unavailable'
        check(actual == expected_qt, 'Provisioned Qt matches manifest: ' + expected_qt)
    else:
        check(False, 'Set ARBITER_QT_PREFIX to the manifest-pinned base Qt kit')
    if sys.platform == 'darwin':
        for name in ('xcrun', 'codesign', 'ditto', 'otool', 'install_name_tool', 'lipo', 'hdiutil'): tool(name)
        import importlib.metadata
        for name, version in (('ds-store', '1.3.1'), ('mac-alias', '2.2.3')):
            try: installed = importlib.metadata.version(name)
            except importlib.metadata.PackageNotFoundError: installed = None
            check(installed == version, name + ' matches tools/release/requirements-macos.txt')
        if shutil.which('xcrun'):
            for component in ('metal', 'actool'):
                result = subprocess.run(['xcrun', '--find', component], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                check(result.returncode == 0, 'Xcode tool: ' + component)
        if a.signing:
            for name in ('ARBITER_MACOS_SIGNING_IDENTITY', 'ARBITER_NOTARY_PROFILE'):
                check(bool(os.environ.get(name)), name + ' is configured (value hidden)')
    elif sys.platform == 'win32':
        check(windows_desktop_available(),
              'Windows GUI tests require an interactive desktop; run from a signed-in '
              'MSVC Developer PowerShell, not a service or SSH/background session')
        for name in ('cl', 'dumpbin'): tool(name)
        directory = Path(os.environ.get('ARBITER_OPENSSL_BIN', '__not_configured__'))
        check(any(directory.glob('libcrypto-3*.dll')), 'OpenSSL runtime DLL is available via ARBITER_OPENSSL_BIN')
        if a.signing:
            tool('signtool')
            check(bool(os.environ.get('ARBITER_WINDOWS_CERT_SHA1')), 'Windows signing certificate selector is configured (value hidden)')
    else:
        for name in ('xvfb-run', 'dbus-run-session', 'gnome-keyring-daemon', 'pkg-config', 'patchelf', 'nm'): tool(name)
        if shutil.which('pkg-config'):
            result = subprocess.run(['pkg-config', '--exists', 'libavcodec', 'libavformat', 'libavutil', 'libsecret-1', 'openssl', 'opus'])
            check(result.returncode == 0, 'System FFmpeg, OpenSSL and Secret Service development packages')
        print('INFO Linux release baseline: Ubuntu 24.04 x64 (Docker or native).')
    print('Preflight: ' + ('ready for native build' if not failures else f'{len(failures)} requirement(s) missing'))
    return bool(failures)


if __name__ == '__main__': sys.exit(main())
