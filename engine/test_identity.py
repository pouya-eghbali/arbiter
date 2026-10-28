import json
from pathlib import Path
import tempfile
import unittest
from identity import fingerprint, verify_sdk


class IdentityTests(unittest.TestCase):
    def test_patches_overlays_and_build_flags_invalidate_sdk(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'engine/patches').mkdir(parents=True)
            (root / 'engine/overlay').mkdir()
            (root / 'dependencies.json').write_text('{}')
            (root / 'engine/manifest.json').write_text('{"qt":"6.12.0"}')
            patch = root / 'engine/patches/qtwebengine.patch'
            patch.write_bytes(b'initial patch\n')
            sdk = root / 'sdk'; sdk.mkdir()
            (sdk / 'arbiter-engine-manifest.json').write_text('{"qt":"6.12.0"}')
            receipt = sdk / 'arbiter-engine-build.json'
            receipt.write_text(json.dumps({'inputs_sha256': fingerprint(root)}))
            verify_sdk(sdk, root)
            patch.write_bytes(patch.read_bytes().replace(b'\n', b'\r\n'))
            verify_sdk(sdk, root)  # Same commit's Windows checkout is equivalent.
            for path in (patch, root / 'engine/overlay/native.cpp', root / 'engine/build.py'):
                path.write_text('changed inputs')
                with self.assertRaisesRegex(RuntimeError, 'rebuild'): verify_sdk(sdk, root)
                receipt.write_text(json.dumps({'inputs_sha256': fingerprint(root)}))
            (sdk / 'arbiter-engine-manifest.json').write_text('{"qt":"6.13.0"}')
            with self.assertRaisesRegex(RuntimeError, 'manifest'): verify_sdk(sdk, root)


if __name__ == '__main__': unittest.main()
