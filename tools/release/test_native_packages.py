import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import publish
import windows_installer


def deb_payload(paths):
    data = io.BytesIO()
    with tarfile.open(fileobj=data, mode='w:xz') as archive:
        for path in paths:
            message = json.dumps({'platform': 'linux', 'commit': 'test'}).encode()
            item = tarfile.TarInfo(path); item.size = len(message)
            archive.addfile(item, io.BytesIO(message))
    payload = data.getvalue()
    header = f'{"data.tar.xz/":<16}{0:<12}{0:<6}{0:<6}{100644:<8}{len(payload):<10}`\n'.encode()
    return b'!<arch>\n' + header + payload


class NativePackageTests(unittest.TestCase):
    def test_reads_deb_identity_and_rejects_ambiguous_or_truncated_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            asset = Path(temporary) / 'test.deb'
            asset.write_bytes(deb_payload(['./opt/arbiter/share/arbiter/release.json']))
            self.assertEqual(publish.package_metadata(asset)['commit'], 'test')
            asset.write_bytes(deb_payload(['./opt/arbiter/share/arbiter/release.json', './unexpected/release.json']))
            with self.assertRaisesRegex(RuntimeError, 'metadata'):
                publish.package_metadata(asset)
            asset.write_bytes(deb_payload(['./opt/arbiter/share/arbiter/release.json'])[:-30])
            with self.assertRaisesRegex(RuntimeError, 'Truncated'):
                publish.package_metadata(asset)

    def test_setup_refuses_wrong_platform_signing_or_missing_executable(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary) / 'bundle'
            metadata = bundle / 'share/arbiter/release.json'
            metadata.parent.mkdir(parents=True)
            with patch.object(windows_installer, 'compiler') as compiler:
                for platform, signed in [('linux', False), ('windows', True), ('windows', False)]:
                    metadata.write_text(json.dumps(dict(version='0.3.1', platform=platform, architecture='x86_64', signed=signed)))
                    with self.assertRaises(RuntimeError):
                        windows_installer.build(bundle, Path(temporary) / 'output', True)
                compiler.assert_not_called()

    def test_nsis_path_escaping_prevents_script_injection(self):
        self.assertEqual(windows_installer.quote('path$"tail'), 'path$$$\\"tail')
        with self.assertRaises(RuntimeError):
            windows_installer.quote('path\nSection')


if __name__ == '__main__':
    unittest.main()
