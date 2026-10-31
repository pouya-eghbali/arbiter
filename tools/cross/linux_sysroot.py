"""Assemble an authenticated Ubuntu x64 SDK without installing/running target code."""
import concurrent.futures
import hashlib
import io
import json
import lzma
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

HERE = Path(__file__).resolve().parent
PINS = json.loads((HERE / 'linux-sysroot.json').read_text())
LOCK = HERE / 'linux-packages.lock.json'


def fetch(url, path, expected=None):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        temporary = path.with_suffix(path.suffix + '.download')
        subprocess.run(['curl', '--fail', '--location', '--silent', '--show-error',
                        '--retry', '3', '--connect-timeout', '20', '--max-time', '180',
                        '--output', str(temporary), url], check=True)
        temporary.replace(path)
    if expected:
        with path.open('rb') as stream:
            actual = hashlib.file_digest(stream, 'sha256').hexdigest()
        if actual != expected:
            path.unlink()
            raise RuntimeError(f'Checksum mismatch: {url}')
    return path


def verify_release(release, keyring, home):
    home.mkdir(mode=0o700, parents=True, exist_ok=True)
    result = subprocess.run(['gpgv', '--homedir', str(home), '--keyring', str(keyring),
                             '--status-fd', '1', str(release)], check=True, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    trusted = False
    for line in result.stdout.splitlines():
        if line.startswith('[GNUPG:] VALIDSIG '):
            fields = line.split()
            trusted |= fields[2] in PINS['signers'] or fields[-1] in PINS['signers']
    if not trusted:
        raise RuntimeError('Ubuntu release was not signed by a pinned archive key')


def indexes(work):
    from debian.deb822 import Packages
    from debian.debian_support import Version
    cache = work / 'downloads'
    keyring = fetch('https://archive.ubuntu.com/ubuntu/project/ubuntu-archive-keyring.gpg', cache / 'ubuntu-archive-keyring.gpg')
    packages, evidence = {}, []
    for pocket in ('noble', 'noble-updates', 'noble-security'):
        base = 'https://security.ubuntu.com/ubuntu/' if pocket.endswith('security') else 'https://archive.ubuntu.com/ubuntu/'
        release = fetch(base + 'dists/' + pocket + '/InRelease', cache / f'{pocket}-InRelease')
        verify_release(release, keyring, work / 'gpg')
        signed = release.read_text()
        for component in ('main', 'universe'):
            name = f'{component}/binary-amd64/Packages.xz'
            match = re.search(r'^ ([0-9a-f]{64})\s+\d+\s+' + re.escape(name) + r'$', signed, re.M)
            if not match:
                raise RuntimeError(f'No signed SHA256 for {pocket}/{name}')
            path = fetch(base + f'dists/{pocket}/{name}', cache / f'{pocket}-{component}-Packages.xz', match[1])
            evidence.append(dict(pocket=pocket, component=component, sha256=match[1]))
            with lzma.open(path, 'rt') as stream:
                for paragraph in Packages.iter_paragraphs(stream, use_apt_pkg=False):
                    if paragraph.get('Architecture') not in ('amd64', 'all'):
                        continue
                    record = {key: paragraph.get(key, '') for key in (
                        'Package', 'Version', 'Architecture', 'Filename', 'SHA256', 'Depends', 'Pre-Depends', 'Provides')}
                    record['base'] = base
                    previous = packages.get(record['Package'])
                    if previous is None or Version(record['Version']) > Version(previous['Version']):
                        packages[record['Package']] = record
            print(f'Verified Ubuntu index: {pocket}/{component}', flush=True)
    return packages, evidence


def resolve(packages, seeds):
    from debian.deb822 import PkgRelation
    from debian.debian_support import Version
    providers = {}
    for name, record in packages.items():
        for group in PkgRelation.parse_relations(record['Provides']) if record.get('Provides') else []:
            for item in group:
                providers.setdefault(item['name'], []).append(name)
    selected, pending = {}, list(seeds)

    def satisfies(record, requirement):
        constraint = requirement.get('version')
        if not constraint:
            return True
        operator, requested = constraint
        actual = Version(record['Version']); requested = Version(requested)
        return {'>=': actual >= requested, '<=': actual <= requested, '=': actual == requested,
                '>>': actual > requested, '<<': actual < requested}[operator]

    while pending:
        name = pending.pop()
        if name in selected:
            continue
        record = packages[name]
        selected[name] = record
        dependencies = ', '.join(filter(None, (record.get('Pre-Depends'), record.get('Depends'))))
        for group in PkgRelation.parse_relations(dependencies) if dependencies else []:
            choice = None
            for requirement in group:
                candidate_names = [requirement['name']] + sorted(providers.get(requirement['name'], []))
                for candidate in candidate_names:
                    if candidate in packages and satisfies(packages[candidate], requirement):
                        choice = candidate
                        break
                if choice:
                    break
            if not choice:
                raise RuntimeError(f'Cannot resolve {name} dependency: {group}')
            pending.append(choice)
    return [selected[name] for name in sorted(selected)]


def lock(work):
    packages, evidence = indexes(work)
    records = resolve(packages, PINS['packages'])
    result = dict(suite=PINS['suite'], architecture=PINS['architecture'], indexes=evidence, packages=records)
    LOCK.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Locked {len(records)} authenticated Ubuntu packages: {LOCK}', flush=True)


def safe_path(root, name):
    relative = PurePosixPath(name)
    if relative.is_absolute() or '..' in relative.parts:
        raise RuntimeError(f'Unsafe archive member: {name}')
    path = root.joinpath(*relative.parts)
    if not path.resolve().is_relative_to(root.resolve()):
        raise RuntimeError(f'Archive member escapes the SDK: {name}')
    return path


def extract_data(archive, root):
    """Extract SDK files and rewrite absolute Linux symlinks inside the sysroot."""
    with tarfile.open(archive, 'r:*') as bundle:
        hardlinks = []
        for member in bundle:
            path = safe_path(root, member.name)
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                path.parent.mkdir(parents=True, exist_ok=True)
                with bundle.extractfile(member) as source, path.open('wb') as target:
                    shutil.copyfileobj(source, target)
                path.chmod(member.mode & 0o777)
            elif member.issym():
                path.parent.mkdir(parents=True, exist_ok=True)
                target = root / member.linkname.lstrip('/') if member.linkname.startswith('/') else path.parent / member.linkname
                if not target.resolve().is_relative_to(root.resolve()):
                    raise RuntimeError(f'Symlink escapes the SDK: {member.name}')
                if path.is_symlink() or path.exists():
                    if path.is_dir() and not path.is_symlink():
                        raise RuntimeError(f'Symlink would overwrite an SDK directory: {path}')
                    path.unlink()
                path.symlink_to(os.path.relpath(target, path.parent))
            elif member.islnk():
                hardlinks.append((path, safe_path(root, member.linkname)))
        for path, target in hardlinks:
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.exists():
                path.unlink()
            os.link(target, path)



def normalize_layout(root):
    # Ubuntu's merged-/usr aliases are normally created by installation scripts.
    # We do not run those scripts; reproduce only the filesystem aliases needed
    # by glibc linker scripts and the standard ELF interpreter path.
    for name in ('lib', 'lib64', 'bin', 'sbin'):
        target = root / 'usr' / name
        target.mkdir(parents=True, exist_ok=True)
        link = root / name
        if link.is_symlink():
            if link.resolve() != target.resolve():
                raise RuntimeError(f'Unexpected sysroot alias: {link}')
        elif link.exists():
            raise RuntimeError(f'Expected merged-/usr sysroot alias: {link}')
        else:
            link.symlink_to('usr/' + name)


def prepare(work):
    if not LOCK.exists():
        raise RuntimeError('Run the lock-sysroot phase first to review/pin Ubuntu package metadata')
    data = json.loads(LOCK.read_text())
    if data['suite'] != PINS['suite'] or data['architecture'] != 'amd64':
        raise RuntimeError('Wrong Linux SDK distribution/architecture')
    root = work / 'sysroot'
    root.mkdir(parents=True, exist_ok=True)
    stamp = root / 'arbiter-sysroot.json'
    expected = {'lock_sha256': hashlib.sha256(LOCK.read_bytes()).hexdigest(),
                'extractor_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    if stamp.exists() and json.loads(stamp.read_text()) == expected:
        return root
    stamp.unlink(missing_ok=True)
    records = data['packages']
    def download(record):
        if not record['Filename'].startswith('pool/') or '..' in PurePosixPath(record['Filename']).parts:
            raise RuntimeError('Invalid Ubuntu package URL')
        archive = fetch(record['base'] + record['Filename'], work / 'downloads' / (record['SHA256'] + '.deb'), record['SHA256'])
        print('Verified target SDK package:', record['Package'], flush=True)
        return archive
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        archives = list(pool.map(download, records))
    for record, archive in zip(records, archives):
        with tempfile.TemporaryDirectory(dir=work) as folder:
            # ar is a native Mac/LLVM tool. No Debian maintainer scripts run.
            names = subprocess.check_output(['ar', 't', str(archive)], text=True).splitlines()
            payload = next(name.strip() for name in names if name.strip().startswith('data.tar'))
            target = Path(folder) / payload
            with target.open('wb') as stream:
                subprocess.run(['ar', 'p', str(archive), payload], check=True, stdout=stream)
            if payload.endswith('.zst'):
                unpacked = Path(folder) / 'data.tar'
                with unpacked.open('wb') as stream:
                    subprocess.run(['zstd', '-dc', str(target)], check=True, stdout=stream)
                target = unpacked
            extract_data(target, root)
        print('Extracted target SDK package:', record['Package'], record['Version'], flush=True)
    normalize_layout(root)
    stamp.write_text(json.dumps(expected, indent=2) + '\n')
    return root


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('lock', 'prepare'))
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    args.work = args.work.resolve()
    args.work.mkdir(parents=True, exist_ok=True)
    print((lock if args.phase == 'lock' else prepare)(args.work))
