import unittest

import linux_abi


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


if __name__ == '__main__':
    unittest.main()
