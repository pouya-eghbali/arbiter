import unittest
from pathlib import Path
import tarfile
import tempfile

import linux_abi
import package


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


if __name__ == '__main__':
    unittest.main()
