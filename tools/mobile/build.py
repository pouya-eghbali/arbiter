#!/usr/bin/env python3
"""Configure/build the Qt mobile app; never upload or erase device data."""
import argparse
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('platform', choices=['preview', 'android', 'ios', 'ios-simulator'])
p.add_argument('--qt-kit', type=Path, required=True)
p.add_argument('--qt-host', type=Path)
p.add_argument('--team', help='Apple development team for device signing')
p.add_argument('--device-testing', action='store_true', help='Omit the managed default-browser entitlement for local testing')
p.add_argument('--release', action='store_true')
p.add_argument('--jobs', type=int, default=min(20, os.cpu_count() or 1))
p.add_argument('--sdk', type=Path, help='Android SDK root')
p.add_argument('--ndk', type=Path, help='Android NDK root')
a = p.parse_args()
build = root / ('build-mobile-qt-' + a.platform)
qt = a.qt_kit.expanduser().resolve()
command = [str(qt / 'bin' / 'qt-cmake'), '-S', str(root / 'mobile'), '-B', str(build)]
config = 'Release' if a.release else 'Debug'
command += ['-DCMAKE_BUILD_TYPE=' + config]
if a.qt_host:
    command += ['-DQT_HOST_PATH=' + str(a.qt_host.expanduser().resolve())]
if a.platform.startswith('ios'):
    command += ['-G', 'Xcode', '-DCMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY=ON',
                '-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0', '-DCMAKE_OSX_SYSROOT=' + ('iphoneos' if a.platform == 'ios' else 'iphonesimulator')]
    if a.team:
        command += ['-DARBITER_APPLE_TEAM=' + a.team]
    command += ['-DARBITER_IOS_DEVICE_TESTING=' + ('ON' if a.device_testing else 'OFF')]
else:
    command += ['-G', 'Ninja']
if a.platform == 'android':
    if not a.sdk or not a.ndk:
        p.error('Android requires --sdk and --ndk')
    command += ['-DANDROID_SDK_ROOT=' + str(a.sdk.resolve()), '-DANDROID_NDK_ROOT=' + str(a.ndk.resolve()),
                '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-28', '-DARBITER_RUST_TARGET=aarch64-linux-android']
if a.platform == 'preview':
    command += ['-DARBITER_MOBILE_TESTS=ON']
subprocess.run(command, check=True)
command = ['cmake', '--build', str(build), '--config', config, '--parallel', str(a.jobs)]
if a.platform == 'ios':
    command += ['--', '-allowProvisioningUpdates', '-allowProvisioningDeviceRegistration']
elif a.platform == 'ios-simulator':
    command += ['--', 'CODE_SIGNING_ALLOWED=NO']
subprocess.run(command, check=True)
print('Build output:', build)
