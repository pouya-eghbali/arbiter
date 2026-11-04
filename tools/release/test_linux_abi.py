import unittest
import os
from pathlib import Path
import tarfile
import tempfile
from unittest.mock import patch

import linux_abi
import package
import linux_rpm


class LinuxAbiTests(unittest.TestCase):
    def test_identify_actual_library_dependencies(self):
        for major, libraries in linux_abi.ABIS.items():
            needed = '\n'.join(f'{name}.so.{version}' for name, version in libraries.items())
            self.assertEqual(linux_abi.identify(needed + '\nlibc.so.6\n'),
                             dict(major=major, libraries=libraries))

    def test_reject_mixed_missing_and_duplicate_abis(self):
        for needed in ('libavcodec.so.62\nlibavformat.so.60\nlibavutil.so.60',
                       'libavcodec.so.62',
                       'libavcodec.so.60\nlibavcodec.so.62\nlibavformat.so.62\nlibavutil.so.60'):
            with self.assertRaises(RuntimeError):
                linux_abi.identify(needed)

    def test_package_names_preserve_baseline_and_distinguish_newer_abis(self):
        self.assertEqual(linux_abi.suffix(6), '')
        self.assertEqual(linux_abi.suffix(8), '-ffmpeg8')
        with self.assertRaises(RuntimeError):
            linux_abi.suffix(99)

    @unittest.skipIf(os.name == 'nt', 'POSIX Linux archive modes')
    def test_native_archive_round_trip_keeps_paths_and_symlinks(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); stage = root / 'arbiter-linux'; stage.mkdir()
            (stage / 'arbiter').write_text('launcher')
            (stage / 'arbiter').chmod(0o755)
            (stage / 'link').symlink_to('arbiter')
            archive = root / 'bundle.tar.xz'
            package.linux_archive(stage, archive)
            with tarfile.open(archive) as tar:
                self.assertEqual(tar.extractfile('arbiter-linux/arbiter').read(), b'launcher')
                self.assertEqual(tar.getmember('arbiter-linux/arbiter').mode, 0o755)
                self.assertEqual(tar.getmember('arbiter-linux/link').linkname, 'arbiter')

    def test_rpm_requires_external_libraries_not_private_qt(self):
        with tempfile.TemporaryDirectory() as folder:
            bundle = Path(folder)
            (bundle / 'libQt6Core.so.6').write_bytes(b'\x7fELF\x02\x01' + bytes(10) + (3).to_bytes(2, 'little'))
            with patch.object(linux_rpm.subprocess, 'check_output', return_value='libQt6Core.so.6\nlibavcodec.so.62\nlibc.so.6\n'):
                self.assertEqual(linux_rpm.dependencies(bundle), ['libavcodec.so.62()(64bit)', 'libc.so.6()(64bit)'])
            with patch.object(linux_rpm.subprocess, 'check_output', return_value='/build/host/libwrong.so\n'):
                with self.assertRaisesRegex(RuntimeError, 'Nonportable'):
                    linux_rpm.dependencies(bundle)


if __name__ == '__main__':
    unittest.main()
