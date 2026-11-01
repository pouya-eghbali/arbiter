#!/usr/bin/env python3
"""Offline tests for manifest merging and mobile release templates."""
import plistlib
from pathlib import Path
import tempfile
import unittest
from merge_privacy import merge

ROOT = Path(__file__).resolve().parents[2]


class PrivacyTests(unittest.TestCase):
    def test_combines_reasons_and_deduplicates(self):
        with tempfile.TemporaryDirectory() as folder:
            files = []
            for index, reasons in enumerate([['CA92.1'], ['CA92.1', '1C8F.1']]):
                path = Path(folder) / str(index)
                path.write_bytes(plistlib.dumps(dict(NSPrivacyAccessedAPITypes=[
                    dict(NSPrivacyAccessedAPIType='NSPrivacyAccessedAPICategoryUserDefaults', NSPrivacyAccessedAPITypeReasons=reasons)])))
                files.append(path)
            data = merge(files)
            self.assertFalse(data['NSPrivacyTracking'])
            self.assertEqual(data['NSPrivacyAccessedAPITypes'][0]['NSPrivacyAccessedAPITypeReasons'], ['1C8F.1', 'CA92.1'])

    def test_sdk_collection_change_requires_review(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'sdk.plist'
            for change in [dict(NSPrivacyTracking=True), dict(NSPrivacyTrackingDomains=['tracking.example']),
                           dict(NSPrivacyCollectedDataTypes=[dict(NSPrivacyCollectedDataType='location')])]:
                path.write_bytes(plistlib.dumps(change))
                with self.assertRaises(ValueError):
                    merge([path])

    def test_app_and_provider_use_generated_versions_and_groups(self):
        for owner in ['App', 'Provider']:
            data = plistlib.loads((ROOT / f'mobile/platform/ios/{owner}-Info.plist').read_bytes())
            self.assertEqual(data['CFBundleShortVersionString'], '@ARBITER_VERSION@')
            self.assertEqual(data['CFBundleVersion'], '@ARBITER_MOBILE_BUILD_NUMBER@')
            self.assertEqual(data['ArbiterAppGroup'], '@ARBITER_IOS_APP_GROUP@')
            manifest = plistlib.loads((ROOT / f'mobile/platform/ios/{owner}Privacy/PrivacyInfo.xcprivacy').read_bytes())
            defaults = next(entry for entry in manifest['NSPrivacyAccessedAPITypes'] if entry['NSPrivacyAccessedAPIType'].endswith('UserDefaults'))
            self.assertEqual(defaults['NSPrivacyAccessedAPITypeReasons'], ['CA92.1', '1C8F.1'])


if __name__ == '__main__':
    unittest.main()
