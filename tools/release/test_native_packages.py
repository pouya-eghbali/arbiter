import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import publish
import windows_installer
from package import normalize_linux_permissions


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
    @unittest.skipIf(os.name == 'nt', 'POSIX package permissions')
    def test_runtime_probe_permissions_are_safe_for_system_installation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = root / 'bundle'; bundle.mkdir(mode=0o700)
            locales = bundle / 'resources/locales'; locales.mkdir(parents=True, mode=0o700)
            data = locales / 'en-US.pak'; data.write_text('fixture'); data.chmod(0o600)
            executable = bundle / 'arbiter'; executable.write_text('fixture'); executable.chmod(0o700)
            outside = root / 'private'; outside.write_text('private'); outside.chmod(0o600)
            (bundle / 'link').symlink_to(outside)
            normalize_linux_permissions(bundle)
            self.assertEqual(locales.stat().st_mode & 0o777, 0o755)
            self.assertEqual(data.stat().st_mode & 0o777, 0o644)
            self.assertEqual(executable.stat().st_mode & 0o777, 0o755)
            self.assertEqual(outside.stat().st_mode & 0o777, 0o600)
            self.assertTrue((bundle / 'link').is_symlink())

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
