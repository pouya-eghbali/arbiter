from datetime import datetime, timedelta, timezone
import unittest
from macos_keychain import validate_profile

class KeychainProfileTest(unittest.TestCase):
    def profile(self):
        return dict(ExpirationDate=datetime.now(timezone.utc)+timedelta(days=1), Platform=['OSX'],
                    ProvisionsAllDevices=True, DeveloperCertificates=[b'certificate'],
                    Entitlements={'com.apple.application-identifier':'TEAM123456.com.arbiter.*',
                                  'keychain-access-groups':['TEAM123456.*']})
    def test_matching_profile(self):
        self.assertEqual(validate_profile(self.profile(),'com.arbiter.browser','TEAM123456',b'certificate'),
                         'TEAM123456.com.arbiter.browser')
    def test_rejects_invalid_profiles(self):
        for change in [dict(ProvisionsAllDevices=False),dict(Platform=['iOS']),
                       dict(ExpirationDate=datetime.now(timezone.utc)-timedelta(days=1)),
                       dict(DeveloperCertificates=[b'another certificate']),
                       dict(Entitlements={'com.apple.application-identifier':'OTHERTEAM.com.arbiter.browser','keychain-access-groups':['*']}),
                       dict(Entitlements={'com.apple.application-identifier':'TEAM123456.*','keychain-access-groups':['OTHERTEAM.*']})]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                validate_profile(self.profile() | change,'com.arbiter.browser','TEAM123456',b'certificate')
