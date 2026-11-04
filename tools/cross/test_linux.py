import io
from pathlib import Path
import struct
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import linux
import linux_ffmpeg
import linux_sysroot as sdk


class LinuxCrossTests(unittest.TestCase):
    def test_current_tumbleweed_ffmpeg_abi_is_explicit(self):
        self.assertEqual(linux_ffmpeg.abi(9), dict(major=9, libraries=dict(
            libavcodec=63, libavformat=63, libavutil=61)))
        self.assertEqual(linux_ffmpeg.PINS['9']['version'], '9.0.2')

    def test_elf_rejects_wrong_architecture_and_macos(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'binary'
            for value in (b'\xcf\xfa\xed\xfe', b'\x7fELF\x02\x01' + b'\0' * 12 + struct.pack('<H', 183)):
                binary.write_bytes(value)
                with self.assertRaises(RuntimeError):
                    linux.elf_x64(binary)
            binary.write_bytes(b'\x7fELF\x02\x01' + b'\0' * 12 + struct.pack('<H', 62))
            linux.elf_x64(binary)

    def test_sysroot_rewrites_absolute_links_and_blocks_escape(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'root'
            root.mkdir()
            archive = Path(folder) / 'package.tar'
            with tarfile.open(archive, 'w') as bundle:
                file = tarfile.TarInfo('usr/lib/example.so.1')
                file.size = 4
                bundle.addfile(file, io.BytesIO(b'ELF!'))
                link = tarfile.TarInfo('usr/lib/example.so')
                link.type = tarfile.SYMTYPE
                link.linkname = '/usr/lib/example.so.1'
                bundle.addfile(link)
            sdk.extract_data(archive, root)
            self.assertEqual((root / 'usr/lib/example.so').read_bytes(), b'ELF!')
            self.assertFalse((root / 'usr/lib/example.so').readlink().is_absolute())
            for name in ('../outside', '/outside'):
                with self.assertRaises(RuntimeError):
                    sdk.safe_path(root, name)
            (root / 'escape').symlink_to(Path(folder))
            with self.assertRaises(RuntimeError):
                sdk.safe_path(root, 'escape/outside')

    def test_ubuntu_layout_is_contained_and_idempotent(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            sdk.normalize_layout(root)
            sdk.normalize_layout(root)
            self.assertEqual((root / 'lib').resolve(), (root / 'usr/lib').resolve())
            (root / 'lib').unlink()
            (root / 'lib').symlink_to('/outside')
            with self.assertRaisesRegex(RuntimeError, 'Unexpected sysroot alias'):
                sdk.normalize_layout(root)

    def test_bad_package_checksum_is_rejected_and_removed(self):
        with tempfile.TemporaryDirectory() as folder:
            package = Path(folder) / 'bad.deb'
            package.write_bytes(b'untrusted')
            with self.assertRaisesRegex(RuntimeError, 'Checksum mismatch'):
                sdk.fetch('https://example.invalid/package', package, '0' * 64)
            self.assertFalse(package.exists())

    def test_archive_signature_requires_pinned_signer(self):
        with tempfile.TemporaryDirectory() as folder:
            home = Path(folder) / 'gpg'
            for signer, allowed in [('0' * 40, False), (sdk.PINS['signers'][0], True)]:
                result = type('Result', (), {'stdout': f'[GNUPG:] VALIDSIG {signer} date timestamp 0 4 0 1 8 01 {signer}\n'})()
                with patch.object(sdk.subprocess, 'run', return_value=result):
                    if allowed:
                        sdk.verify_release(Path('release'), Path('keyring'), home)
                    else:
                        with self.assertRaisesRegex(RuntimeError, 'pinned archive key'):
                            sdk.verify_release(Path('release'), Path('keyring'), home)

    def test_dependency_closure_selects_version_and_virtual_provider(self):
        def package(name, version='1', depends='', provides=''):
            return dict(Package=name, Version=version, Depends=depends, Provides=provides)
        packages = {p['Package']: p for p in [
            package('app', depends='old (>= 2) | current (>= 2), virtual'),
            package('old'), package('current', '3'), package('implementation', provides='virtual'),
            package('0-unrelated')]}
        self.assertEqual([p['Package'] for p in sdk.resolve(packages, ['app'])],
                         ['app', 'current', 'implementation'])
        packages['app']['Depends'] = 'missing'
        with self.assertRaisesRegex(RuntimeError, 'Cannot resolve'):
            sdk.resolve(packages, ['app'])


if __name__ == '__main__':
    unittest.main()
