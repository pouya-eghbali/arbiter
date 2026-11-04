#!/usr/bin/env python3
"""Build a per-user Windows setup executable from a verified portable bundle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent


def quote(value):
    if '\n' in str(value) or '\r' in str(value):
        raise RuntimeError('Installer paths cannot contain line breaks')
    return str(value).replace('$', '$$').replace('"', '$\\"')


def compiler():
    found = shutil.which('makensis')
    if found:
        return found
    for variable in ('ProgramFiles(x86)', 'ProgramFiles'):
        path = Path(os.environ.get(variable, '/nonexistent')) / 'NSIS/makensis.exe'
        if path.is_file():
            return str(path)
    raise RuntimeError('Install NSIS (makensis) to build Windows setup executables')


def build(bundle, output, unsigned):
    bundle, output = bundle.resolve(), output.resolve()
    metadata = json.loads((bundle / 'share/arbiter/release.json').read_text())
    version = metadata['version']
    if metadata['platform'] != 'windows' or metadata['architecture'] != 'x86_64' or not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise RuntimeError('Expected a stable Windows x86_64 bundle')
    if metadata['signed'] == unsigned:
        raise RuntimeError('Installer signing mode must match the portable application')
    if not unsigned and (sys.platform != 'win32' or not os.environ.get('ARBITER_WINDOWS_CERT_SHA1')):
        raise RuntimeError('Signed Windows setup must be built on Windows with ARBITER_WINDOWS_CERT_SHA1')
    if not (bundle / 'bin/arbiter.exe').is_file():
        raise RuntimeError('Application executable is missing')
    files = sorted(p for p in bundle.rglob('*') if p.is_file())
    if any(not p.resolve().is_relative_to(bundle) for p in files):
        raise RuntimeError('Installer bundle contains an escaping symlink')
    output.mkdir(parents=True, exist_ok=True)
    target = output / f'arbiter-{version}-windows-x86_64{"-unsigned" if unsigned else ""}-setup.exe'
    if target.exists():
        raise RuntimeError('Refusing to overwrite ' + str(target))
    copies, deletes = [], []
    directories = set()
    for p in files:
        relative = p.relative_to(bundle)
        folder = str(relative.parent).replace('/', '\\')
        copies += ['  SetOutPath "$INSTDIR' + ('' if folder == '.' else '\\' + quote(folder)) + '"',
                   '  File "' + quote(p) + '"']
        deletes.append('  Delete "$INSTDIR\\' + quote(str(relative).replace('/', '\\')) + '"')
        directories.update(parent for parent in relative.parents if parent != Path('.'))
    script = (HERE / 'windows/setup.nsi.in').read_text()
    values = dict(OUTPUT=quote(target), VERSION=version, SIZE=str(sum(p.stat().st_size for p in files) // 1024),
        FILES='\n'.join(copies), DELETES='\n'.join(deletes),
        DIRECTORIES='\n'.join('  RMDir "$INSTDIR\\' + quote(str(p).replace('/', '\\')) + '"'
                             for p in sorted(directories, key=lambda p: (-len(p.parts), str(p)))))
    for key, value in values.items():
        script = script.replace('@' + key + '@', value)
    with tempfile.TemporaryDirectory(prefix='arbiter-nsis-') as temporary:
        source = Path(temporary) / 'setup.nsi'; source.write_text(script, encoding='utf-8')
        prefix = '/' if sys.platform == 'win32' else '-'
        subprocess.run([compiler(), prefix + 'WX', prefix + 'V2', str(source)], check=True)
    if not unsigned:
        subprocess.run(['signtool', 'sign', '/sha1', os.environ['ARBITER_WINDOWS_CERT_SHA1'], '/fd', 'SHA256',
            '/tr', os.environ.get('ARBITER_TIMESTAMP_URL', 'http://timestamp.digicert.com'), '/td', 'SHA256', str(target)], check=True)
        subprocess.run(['signtool', 'verify', '/pa', str(target)], check=True)
    with target.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    target.with_name(target.name + '.sha256').write_text(digest + '  ' + target.name + '\n')
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--unsigned', action='store_true')
    args = parser.parse_args()
    print(build(args.bundle, args.output, args.unsigned))


if __name__ == '__main__':
    main()
