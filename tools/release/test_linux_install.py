import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / 'tools/release/linux'
spec = importlib.util.spec_from_file_location('linux_install', SCRIPTS / 'install.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class LinuxInstallTests(unittest.TestCase):
    def bundle(self, root):
        source = root / 'download'
        (source / 'bin').mkdir(parents=True)
        (source / 'share/arbiter').mkdir(parents=True)
        (source / 'share/applications').mkdir(parents=True)
        for name in ('arbiter', 'install.sh', 'install.py'):
            shutil.copy2(SCRIPTS / name, source / name)
        (source / 'arbiter').chmod(0o755)
        (source / 'bin/arbiter').write_text('#!/bin/sh\nprintf "<%s>\\n" "$@"\n')
        (source / 'bin/arbiter').chmod(0o755)
        (source / 'share/arbiter/arbiter.svg').write_text('<svg/>')
        shutil.copy2(ROOT / 'cmake/com.arbiter.browser.desktop',
                     source / 'share/applications/com.arbiter.browser.desktop')
        return source

    @patch.object(installer, 'refresh')
    def test_install_update_launch_and_uninstall_keep_profile(self, refresh):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); source = self.bundle(root)
            paths = installer.locations(root / "prefix with spaces ' quote $ percent%")
            profile = root / 'profile'; profile.mkdir(); (profile / 'passwords').write_text('keep')
            installer.install(source, paths)
            self.assertIn('Icon=com.arbiter.browser', paths['desktop'].read_text())
            self.assertIn('percent%%', paths['desktop'].read_text())
            # Use a fake ldd to test the shell wrapper's actual argument forwarding.
            tools = root / 'tools'; tools.mkdir()
            (tools / 'ldd').write_text('#!/bin/sh\nexit 0\n'); (tools / 'ldd').chmod(0o755)
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'])
            result = subprocess.run([str(paths['command']), 'https://example.org/?q=a b', '--flag'],
                                    env=env, text=True, capture_output=True, check=True)
            self.assertEqual(result.stdout, '<https://example.org/?q=a b>\n<--flag>\n')
            (source / 'version').write_text('new')
            installer.install(source, paths)
            self.assertEqual((paths['app'] / 'version').read_text(), 'new')
            installer.uninstall(paths)
            self.assertFalse(any(p.exists() for p in paths.values()))
            self.assertEqual((profile / 'passwords').read_text(), 'keep')

    def test_refuse_unowned_directory_and_symlink(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); source = self.bundle(root); paths = installer.locations(root / 'prefix')
            paths['app'].mkdir(parents=True)
            sentinel = paths['app'] / 'data'; sentinel.write_text('keep')
            with self.assertRaisesRegex(RuntimeError, 'outside this installer'):
                installer.install(source, paths)
            with self.assertRaisesRegex(RuntimeError, 'No installation'):
                installer.uninstall(paths)
            self.assertEqual(sentinel.read_text(), 'keep')
            shutil.rmtree(paths['app']); paths['app'].symlink_to(source)
            with self.assertRaisesRegex(RuntimeError, 'symlink'):
                installer.install(source, paths)

    def test_xdg_data_home_and_explicit_prefix(self):
        with patch.object(Path, 'home', return_value=Path('/test/home')), \
             patch.dict(os.environ, XDG_DATA_HOME='/custom/data'):
            self.assertEqual(installer.locations()['app'], Path('/custom/data/arbiter'))
            self.assertEqual(installer.locations('/opt/local')['app'], Path('/opt/local/share/arbiter'))

    def test_install_destination_cannot_recurse_into_download(self):
        with tempfile.TemporaryDirectory() as folder:
            source = self.bundle(Path(folder))
            with self.assertRaisesRegex(RuntimeError, 'must be separate'):
                installer.install(source, installer.locations(source / 'nested'))

    def test_desktop_quoting_escapes_both_layers(self):
        self.assertEqual(installer.desktop_quote('/a b/$c`d"e\\f%'),
                         '"/a b/\\\\$c\\\\`d\\\\"e\\\\\\\\f%%"')

    def test_launcher_reports_missing_abi_without_running_app(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); source = self.bundle(root); tools = root / 'tools'; tools.mkdir()
            ldd = tools / 'ldd'
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'])
            for output in ('libavcodec.so.60 => not found', "version `GLIBC_2.39' not found"):
                ldd.write_text('#!/bin/sh\nprintf "%s\\n" ' + installer.shlex.quote(output) + '\nexit 1\n')
                ldd.chmod(0o755)
                result = subprocess.run(['/bin/sh', str(source / 'arbiter'), '--check-dependencies'],
                                        env=env, text=True, capture_output=True)
                self.assertEqual(result.returncode, 1)
                self.assertIn(output, result.stderr)
                self.assertNotIn('<--check-dependencies>', result.stdout)


if __name__ == '__main__':
    unittest.main()
