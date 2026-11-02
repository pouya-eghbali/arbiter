"""Validate a Developer ID profile before adding protected-Keychain entitlements."""
from datetime import datetime, timezone
import fnmatch
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile


def validate_profile(profile, bundle, team, certificate, now=None):
    now = now or datetime.now(timezone.utc)
    expiration = profile.get('ExpirationDate')
    if expiration is None or expiration.replace(tzinfo=timezone.utc) <= now:
        raise ValueError('Provisioning profile is expired')
    if 'OSX' not in profile.get('Platform', []) or not profile.get('ProvisionsAllDevices'):
        raise ValueError('A Developer ID profile valid for all Macs is required')
    identifier = team + '.' + bundle
    entitlements = profile.get('Entitlements', {})
    if not fnmatch.fnmatchcase(identifier, entitlements.get('com.apple.application-identifier', '')):
        raise ValueError('Provisioning profile does not authorize this app identity')
    if not any(fnmatch.fnmatchcase(identifier, group) for group in entitlements.get('keychain-access-groups', [])):
        raise ValueError('Provisioning profile does not authorize this app Keychain group')
    if certificate not in profile.get('DeveloperCertificates', []):
        raise ValueError('Provisioning profile does not authorize the signing certificate')
    return identifier


def configure(app, info, team, entitlements):
    # Never add restricted entitlements without an authorizing profile: macOS
    # otherwise terminates the app before main(). Ad-hoc builds keep the fallback.
    entitlements.pop('com.apple.application-identifier', None)
    entitlements.pop('keychain-access-groups', None)
    embedded = app / 'Contents/embedded.provisionprofile'
    explicit = os.environ.get('ARBITER_MACOS_PROVISIONING_PROFILE')
    if explicit and not team:
        raise ValueError('A Developer ID identity is required for the provisioning profile')
    if not team:
        embedded.unlink(missing_ok=True)
        return False
    paths = [Path(explicit).expanduser()] if explicit else sorted(
        (Path.home() / 'Library/Developer/Xcode/UserData/Provisioning Profiles').glob('*'))
    with tempfile.TemporaryDirectory(prefix='arbiter-signing-certificate-') as directory:
        prefix = str(Path(directory) / 'certificate')
        subprocess.run(['codesign', '-d', '--extract-certificates=' + prefix,
                        str(app / 'Contents/MacOS' / info['CFBundleExecutable'])],
                       check=True, capture_output=True)
        certificate = Path(prefix + '0').read_bytes()
    for path in paths:
        try:
            value = subprocess.run(['security', 'cms', '-D', '-i', str(path)], check=True, capture_output=True).stdout
            profile = plistlib.loads(value)
            identifier = validate_profile(profile, info['CFBundleIdentifier'], team, certificate)
        except (ValueError, OSError, subprocess.CalledProcessError, plistlib.InvalidFileException):
            if explicit:
                raise ValueError('The selected macOS provisioning profile is not valid for this Developer ID app') from None
            continue
        shutil.copy2(path, embedded)
        entitlements['com.apple.application-identifier'] = identifier
        entitlements['keychain-access-groups'] = [identifier]
        return True
    embedded.unlink(missing_ok=True)
    return False
