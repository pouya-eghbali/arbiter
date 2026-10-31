import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch
from urllib.error import HTTPError

spec = importlib.util.spec_from_file_location('security_dependencies', Path(__file__).resolve().parents[1] / 'dependencies.py')
dependencies = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dependencies)


class MaintenanceSecurityTests(unittest.TestCase):
    def test_missing_new_tag_still_checks_release_branch(self):
        manifest = {'webengine_version': '6.140.0', 'webengine_ref': '6.140.0'}
        with patch.object(dependencies, 'api', return_value={'sha': 'new'}) as api:
            self.assertEqual(dependencies.maintenance_ref(manifest, None), '6.140.0')
            api.assert_called_once_with('qtwebengine/commits/6.140.0')

    def test_new_stable_feature_release_takes_precedence(self):
        self.assertEqual(dependencies.maintenance_ref({'webengine_version': '6.140.0'}, '6.141.0'), 'v6.141.0')

    def test_network_or_rate_limit_errors_are_not_reported_as_up_to_date(self):
        error = HTTPError('https://api.github.com', 403, 'Rate limited', {}, None)
        with patch.object(dependencies, 'api', side_effect=error):
            with self.assertRaises(HTTPError):
                dependencies.maintenance_ref({'webengine_version': '6.140.0'}, None)

    def test_same_line_security_snapshot_is_allowed_but_security_downgrade_is_rejected(self):
        manifest = {'webengine_version': '6.140.0', 'chromium_security_version': '155.0.8059.85'}
        def content(repo, path, ref):
            if path == '.cmake.conf':
                return 'set(QT_REPO_MODULE_VERSION "6.140.0")\nset(QT_REPO_MODULE_PRERELEASE_VERSION_SEGMENT "alpha1")'
            if path == 'CHROMIUM_VERSION':
                return 'Patched with security patches up to Chromium version: 156.0.8078.74'
            return 'MAJOR=140\nMINOR=0\nBUILD=7339\nPATCH=264'
        with patch.object(dependencies, 'api', return_value={'sha': 'a' * 40}), patch.object(dependencies, 'content', side_effect=content):
            result = dependencies.propose(manifest, '6.12.0', '6.140.0')
            self.assertEqual(result['chromium_security_version'], '156.0.8078.74')
            self.assertEqual(result['webengine_ref'], '6.140.0')
            self.assertEqual(result['webengine_prerelease'], 'alpha1')
            with self.assertRaisesRegex(RuntimeError, 'prerelease'):
                dependencies.propose(manifest, '6.12.0', 'dev')
            manifest['chromium_security_version'] = '157.0.1.1'
            with self.assertRaisesRegex(RuntimeError, 'downgrade'):
                dependencies.propose(manifest, '6.12.0', '6.140.0')
