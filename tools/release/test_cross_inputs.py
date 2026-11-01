import hashlib
import importlib.util
import os
import json
from pathlib import Path
import stat
import sys
import types
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location('cross_inputs', Path(__file__).with_name('cross_inputs.py'))
inputs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inputs)


class CrossInputTests(unittest.TestCase):
    def test_extension_probe_never_builds_a_release_package(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(inputs.subprocess, 'run') as run, \
             patch.object(inputs, 'WORK', Path(directory)):
            inputs.extension_probe({'ARBITER_QT_PREFIX': str(Path(directory) / 'qt')})
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(len(commands), 3)
            self.assertIn('extensions_test', commands[1])
            self.assertEqual(commands[2][0], 'ctest')
            self.assertTrue(all('--package' not in command for command in commands))

    def test_extension_probe_rejects_other_platform_before_any_build(self):
        with patch.dict(os.environ, dict(INPUT_PLATFORM='linux', ARBITER_CROSS_EXTENSION_PROBE='1')), \
             patch.object(inputs.sys, 'platform', 'linux'), patch.object(inputs.subprocess, 'run') as run:
            with self.assertRaisesRegex(RuntimeError, 'requires Windows'):
                inputs.build()
            run.assert_not_called()

    def test_untrusted_sdk_archives_cannot_escape_validation_directory(self):
        for name in ('../outside', '/sdk/file', 'sdk/../../outside', 'sdk\\..\\..\\outside', 'C:/sdk/file', 'sdk/file:stream'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                archive = root / 'build-inputs-linux.zip'
                with zipfile.ZipFile(archive, 'w') as bundle:
                    # ZipInfo's constructor normalizes host separators on
                    # Windows; preserve the deliberately malicious raw name.
                    member = zipfile.ZipInfo('fixture')
                    member.filename = name
                    bundle.writestr(member, 'untrusted')
                env = dict(INPUT_PLATFORM='linux', RELEASE_TAG='v0.2.1',
                           INPUT_SHA256=hashlib.sha256(archive.read_bytes()).hexdigest(), GITHUB_REPOSITORY='owner/repo')
                with patch.object(inputs, 'WORK', root), patch.dict(os.environ, env), \
                     patch.object(inputs.subprocess, 'check_output', side_effect=[json.dumps(dict(isDraft=True,targetCommitish='a'*40)), 'a'*40]), \
                     patch.object(inputs.subprocess, 'run'):
                    with self.assertRaisesRegex(RuntimeError, 'Unsafe'):
                        inputs.download()

    @unittest.skipUnless(os.name == 'posix', 'POSIX executable modes require a POSIX filesystem')
    def test_linux_helper_retains_executable_permissions(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            archive = root / 'build-inputs-linux.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                helper = zipfile.ZipInfo('sdk/libexec/QtWebEngineProcess')
                helper.external_attr = (stat.S_IFREG | 0o755) << 16
                bundle.writestr(helper, b'fixture')
            env = dict(INPUT_PLATFORM='linux', RELEASE_TAG='v0.2.1',
                       INPUT_SHA256=hashlib.sha256(archive.read_bytes()).hexdigest(), GITHUB_REPOSITORY='owner/repo')
            with patch.object(inputs, 'WORK', root), patch.dict(os.environ, env), \
                 patch.object(inputs.subprocess, 'check_output', side_effect=[json.dumps(dict(isDraft=True,targetCommitish='a'*40)), 'a'*40]), \
                 patch.object(inputs.subprocess, 'run'):
                inputs.download()
            self.assertEqual((root / helper.filename).stat().st_mode & 0o777, 0o755)

    def test_published_release_cannot_be_used_for_private_build_inputs(self):
        env = dict(INPUT_PLATFORM='linux', RELEASE_TAG='v0.2.1', INPUT_SHA256='a'*64, GITHUB_REPOSITORY='owner/repo')
        with patch.dict(os.environ, env), \
             patch.object(inputs.subprocess, 'check_output', side_effect=[json.dumps(dict(isDraft=False,targetCommitish='a'*40)), 'a'*40]), \
             patch.object(inputs.subprocess, 'run') as download:
            with self.assertRaisesRegex(RuntimeError, 'draft pinned'):
                inputs.download()
            download.assert_not_called()

    def test_sdk_must_match_exact_release_commit(self):
        env = dict(INPUT_PLATFORM='linux', RELEASE_TAG='v0.2.1', INPUT_SHA256='a' * 64, GITHUB_REPOSITORY='owner/repo')
        with patch.dict(os.environ, env), \
             patch.object(inputs.subprocess, 'check_output', side_effect=[json.dumps(dict(isDraft=True,targetCommitish='a'*40)), 'b'*40]), \
             patch.object(inputs.subprocess, 'run') as download:
            with self.assertRaisesRegex(RuntimeError, 'exact checked-out release'):
                inputs.download()
            download.assert_not_called()

    def test_sdk_packaging_never_follows_a_link_outside_the_sdk(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'sdk').mkdir()
            private = root / 'outside.txt'
            private.write_text('fixture outside the SDK')
            (root / 'sdk/escape').symlink_to(private)
            identity = types.ModuleType('identity')
            identity.verify_sdk = lambda _: dict(platform='linux',architecture='x86_64',inputs_sha256='a'*64)
            with patch.object(inputs, 'ROOT', root), patch.dict(os.environ, dict(INPUT_PLATFORM='linux', ARBITER_CROSS_WORK=str(root))), \
                 patch.dict(sys.modules, {'identity': identity}), patch.object(sys, 'path', sys.path[:]):
                with self.assertRaisesRegex(RuntimeError, 'escapes its root'):
                    inputs.pack()
            self.assertFalse((root / 'build-release-artifacts/build-inputs-linux.zip').exists())

    def test_compilation_is_never_run_on_wrong_platform(self):
        with patch.dict(os.environ, {'INPUT_PLATFORM': 'windows'}), \
             patch.object(inputs.sys, 'platform', 'darwin'), patch.object(inputs.subprocess, 'run') as run:
            with self.assertRaisesRegex(RuntimeError, 'target platform'):
                inputs.build()
            run.assert_not_called()
