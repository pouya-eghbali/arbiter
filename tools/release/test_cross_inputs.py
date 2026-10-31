import hashlib
import importlib.util
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location('cross_inputs', Path(__file__).with_name('cross_inputs.py'))
inputs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inputs)


class CrossInputTests(unittest.TestCase):
    def test_untrusted_sdk_archives_cannot_escape_validation_directory(self):
        for name in ('../outside', '/sdk/file', 'sdk/../../outside', 'sdk\\outside', 'C:/sdk/file'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                archive = root / 'build-inputs-linux.zip'
                with zipfile.ZipFile(archive, 'w') as bundle:
                    bundle.writestr(name, 'untrusted')
                env = dict(INPUT_PLATFORM='linux', RELEASE_TAG='v0.2.1',
                           INPUT_SHA256=hashlib.sha256(archive.read_bytes()).hexdigest(), GITHUB_REPOSITORY='owner/repo')
                with patch.object(inputs, 'WORK', root), patch.dict(os.environ, env), \
                     patch.object(inputs.subprocess, 'check_output', return_value='a' * 40), \
                     patch.object(inputs.subprocess, 'run'):
                    with self.assertRaisesRegex(RuntimeError, 'Unsafe'):
                        inputs.download()

    def test_sdk_must_match_exact_release_commit(self):
        env = dict(INPUT_PLATFORM='linux', RELEASE_TAG='v0.2.1', INPUT_SHA256='a' * 64, GITHUB_REPOSITORY='owner/repo')
        with patch.dict(os.environ, env), \
             patch.object(inputs.subprocess, 'check_output', side_effect=['a' * 40, 'b' * 40]), \
             patch.object(inputs.subprocess, 'run') as download:
            with self.assertRaisesRegex(RuntimeError, 'exact checked-out release'):
                inputs.download()
            download.assert_not_called()

    def test_compilation_is_never_run_on_wrong_platform(self):
        with patch.dict(os.environ, {'INPUT_PLATFORM': 'windows'}), \
             patch.object(inputs.sys, 'platform', 'darwin'), patch.object(inputs.subprocess, 'run') as run:
            with self.assertRaisesRegex(RuntimeError, 'target platform'):
                inputs.build()
            run.assert_not_called()
