#!/usr/bin/env python3
"""Upload verified native assets and matching source to a GitHub DRAFT only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def run(*args, capture=False):
    result = subprocess.run([str(a) for a in args], cwd=ROOT, check=True, text=True,
                            stdout=subprocess.PIPE if capture else None)
    return result.stdout


def package_metadata(asset):
    if asset.name.endswith('.zip'):
        with zipfile.ZipFile(asset) as archive:
            members = [m for m in archive.infolist() if m.filename.endswith('/release.json')]
            if len(members) != 1 or members[0].file_size > 65536:
                raise RuntimeError(f'Invalid release metadata in {asset.name}')
            return json.loads(archive.read(members[0]))
    with tarfile.open(asset, 'r:xz') as archive:
        members = [m for m in archive.getmembers() if m.name.endswith('/release.json')]
        if len(members) != 1 or not members[0].isfile() or members[0].size > 65536:
            raise RuntimeError(f'Invalid release metadata in {asset.name}')
        return json.load(archive.extractfile(members[0]))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--tag', default=os.environ.get('RELEASE_TAG'), required=not os.environ.get('RELEASE_TAG'))
    p.add_argument('--assets', type=Path, default=ROOT / 'build-release-artifacts')
    p.add_argument('--unsigned', action='store_true')
    p.add_argument('--prepare-only', action='store_true', help='Create a source-only draft; binaries are uploaded after local builds')
    p.add_argument('--platform', action='append', choices=('macos', 'windows', 'linux'),
                   help='Upload only this platform; repeat to select several. Default: require all three')
    a = p.parse_args()
    if a.prepare_only and a.platform: p.error('--prepare-only cannot be combined with --platform')
    tag = a.tag
    import re
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:-[A-Za-z0-9]+(?:\.[A-Za-z0-9]+)*)?', tag):
        p.error('Expected a version tag such as v0.1.0-rc.1')
    sha = run('git', 'rev-parse', '--verify', f'refs/tags/{tag}^{{commit}}', capture=True).strip()
    if run('git', 'rev-parse', 'HEAD', capture=True).strip() != sha:
        raise RuntimeError('Check out the release tag before collecting/uploading its packages')
    if run('git', 'show', f'{sha}:VERSION.txt', capture=True).strip() != tag[1:]:
        raise RuntimeError('Release tag does not match VERSION.txt')
    if os.environ.get('RELEASE_SHA', sha) != sha: raise RuntimeError('Release commit mismatch')
    output = a.assets.resolve()
    output.mkdir(parents=True, exist_ok=True)
    repo = os.environ.get('GITHUB_REPOSITORY') or run('gh', 'repo', 'view', '--json', 'nameWithOwner', '--jq', '.nameWithOwner', capture=True).strip()
    # Check the remote tag again in case it moved during the long native build.
    remote_sha = run('gh', 'api', f'repos/{repo}/commits/{tag}', '--jq', '.sha', capture=True).strip()
    if remote_sha != sha: raise RuntimeError('Remote tag moved during the build; refusing to publish')
    version = tag[1:]
    unsigned = a.unsigned or os.environ.get('RELEASE_UNSIGNED') == 'true'
    suffix = '-unsigned' if unsigned else ''
    platforms = set(a.platform or ('macos', 'windows', 'linux'))
    partial = bool(a.platform)
    assets = [] if a.prepare_only else [output / f'arbiter-{version}-{os_name}-{arch}{suffix if os_name != "linux" else ""}.{extension}'
              for os_name, arch, extension in [('macos', 'arm64', 'zip'),
                                               ('windows', 'x86_64', 'zip'), ('linux', 'x86_64', 'tar.xz')] if os_name in platforms]
    for asset in assets:
        expected = (output / (asset.name + '.sha256')).read_text().split()[0]
        with asset.open('rb') as stream: actual = hashlib.file_digest(stream, 'sha256').hexdigest()
        if expected != actual: raise RuntimeError(f'Artifact checksum mismatch: {asset.name}')
        metadata = package_metadata(asset)
        if metadata['commit'] != sha or metadata['version'] != version:
            raise RuntimeError(f'Package was built from a different commit/version: {asset.name}')
        if metadata['dependencies'] != json.loads((ROOT / 'dependencies.json').read_text()):
            raise RuntimeError(f'Package dependency pins differ: {asset.name}')
        import sys
        sys.path.insert(0, str(ROOT / 'engine'))
        from identity import fingerprint
        if metadata['engine']['inputs_sha256'] != fingerprint():
            raise RuntimeError(f'Package engine inputs differ: {asset.name}')
        if metadata['platform'] == 'macos':
            pin = json.loads((ROOT / 'tools/release/openssl.json').read_text())
            if not metadata.get('minimum_os') or any(metadata.get('openssl', {}).get(key) != value for key, value in pin.items()):
                raise RuntimeError(f'Package OpenSSL pin or macOS minimum differs: {asset.name}')
        expected_signed = metadata['platform'] != 'linux' and not unsigned
        expected_name = f'arbiter-{version}-{metadata["platform"]}-{metadata["architecture"]}{suffix if metadata["platform"] != "linux" else ""}'
        if metadata['signed'] != expected_signed or not asset.name.startswith(expected_name + '.'):
            raise RuntimeError(f'Package platform, architecture or signing status differs: {asset.name}')
    source = output / f'arbiter-{version}-source.tar.gz'
    run('git', 'archive', '--format=tar.gz', f'--prefix=arbiter-{version}/', f'--output={source}', sha)
    assets.append(source)
    sums = output / ('SHA256SUMS-' + '-'.join(sorted(platforms)) if partial else 'SHA256SUMS')
    lines = []
    for asset in assets:
        with asset.open('rb') as stream: digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        lines.append(f'{digest}  {asset.name}\n')
    sums.write_text(''.join(lines))
    assets.append(sums)
    # Sidecars make independently uploaded packages verifiable without a combined manifest.
    assets.extend(output / (asset.name + '.sha256') for asset in list(assets)
                  if asset.name.endswith(('.zip', '.tar.xz')))
    notes = output / 'release-notes.md'
    notes.write_text(f'''Arbiter {version} — {'SOURCE-ONLY DRAFT — local binaries pending' if a.prepare_only else 'UNSIGNED TESTING DRAFT' if unsigned else 'PARTIAL DRAFT — other platform packages may be pending' if partial else 'release candidate'}

Commit: `{sha}`

Packages: macOS (Apple Silicon; minimum OS in package metadata), Windows x64, Linux x64
(Ubuntu 24.04 build baseline). Extract the Windows ZIP and run `bin/arbiter.exe`;
extract the Linux archive and run `./arbiter`. Linux requires the system FFmpeg,
OpenSSL 3, desktop Qt platform dependencies and a Secret Service/KWallet provider.
Windows requires the included/installed Microsoft VC++ runtime; Windows Hello
vault verification requires Windows 11. Native platform/hardware validation is
required before publishing this draft.

Use the SHA256SUMS manifests or package .sha256 sidecars to verify downloads. The source archive includes Arbiter, the
engine patches and exact dependency pins. It is not a complete dependency source
bundle. Before making binaries public, provide corresponding dependency sources
and notices as required by their licenses; see docs/releases.md.

Arbiter does not bundle or download Widevine. When available, it can use a
compatible Widevine module already installed on your device. Widevine is
proprietary Google software governed by applicable Google license terms.
Compatibility with streaming services and playback quality are not guaranteed.

This draft is never published automatically. Review native authentication,
passkeys/phone QR, extensions, ad blocking, media playback, permissions, and a
launch on a clean machine without a Qt SDK before publishing.
''')
    existing = subprocess.run(['gh', 'release', 'view', tag, '--repo', repo, '--json', 'isDraft'], cwd=ROOT,
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if existing.returncode == 0:
        if not json.loads(existing.stdout)['isDraft']:
            raise RuntimeError('Refusing to replace assets on a published release')
        if a.prepare_only:
            print('Existing draft preserved. Upload local packages without --prepare-only when ready.')
            return
        if not partial:
            run('gh', 'release', 'edit', tag, '--repo', repo, '--draft',
                '--prerelease=' + str('-' in tag or unsigned).lower(), '--notes-file', notes)
        run('gh', 'release', 'upload', tag, '--repo', repo, *assets, '--clobber')
    else:
        args = ['gh', 'release', 'create', tag, '--repo', repo, '--draft', '--verify-tag', '--title', 'Arbiter ' + version, '--notes-file', notes]
        if '-' in tag or unsigned: args += ['--prerelease']
        run(*args, *assets)


if __name__ == '__main__': main()
