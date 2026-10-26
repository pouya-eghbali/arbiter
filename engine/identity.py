"""Content identity of all inputs that can change Arbiter's patched SDK."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def fingerprint(root=ROOT):
    paths = [root / 'dependencies.json']
    paths += [p for p in (root / 'engine').rglob('*') if p.is_file()
              and '__pycache__' not in p.parts and p.suffix != '.pyc'
              and not p.name.startswith('test_') and p.name != '.gitignore']
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(path.relative_to(root).as_posix().encode() + b'\0')
        data = path.read_bytes()
        if b'\0' not in data:
            data = data.replace(b'\r\n', b'\n')
        digest.update(data + b'\0')
    return digest.hexdigest()


def verify_sdk(sdk, root=ROOT):
    expected = json.loads((root / 'engine/manifest.json').read_text())
    if json.loads((sdk / 'arbiter-engine-manifest.json').read_text()) != expected:
        raise RuntimeError('SDK manifest differs from the application engine pins')
    receipt = json.loads((sdk / 'arbiter-engine-build.json').read_text())
    if receipt['inputs_sha256'] != fingerprint(root):
        raise RuntimeError('SDK patch/overlay/build inputs differ; rebuild this SDK')
    return receipt


if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--verify', type=Path)
    a = p.parse_args()
    if a.verify:
        print(json.dumps(verify_sdk(a.verify), indent=2))
    else:
        print(fingerprint())
