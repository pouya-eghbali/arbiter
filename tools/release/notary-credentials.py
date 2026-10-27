#!/usr/bin/env python3
"""Store local Apple credentials without printing their values."""
import os
import subprocess


def main():
    result = subprocess.run([
        'xcrun', 'notarytool', 'store-credentials', os.environ['ARBITER_NOTARY_PROFILE'],
        '--apple-id', os.environ['APPLE_ID'], '--team-id', os.environ['APPLE_TEAM_ID'],
        '--password', os.environ['APPLE_APP_PASSWORD'],
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise SystemExit('Could not store/validate notarization credentials. Check Apple credentials and keychain access; output hidden to protect secrets.')
    print('Notarization credentials validated and stored in the login keychain.')


if __name__ == '__main__': main()
