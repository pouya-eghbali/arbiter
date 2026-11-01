#!/usr/bin/env python3
"""Configure/build the Qt mobile app; never upload or erase device data."""
import argparse
import os
from pathlib import Path
import subprocess
import re

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('platform', choices=['preview', 'android', 'ios', 'ios-simulator'])
p.add_argument('--qt-kit', type=Path, required=True)
p.add_argument('--qt-host', type=Path)
p.add_argument('--team', help='Apple development team for device signing')
p.add_argument('--device-testing', action='store_true', help='Compatibility alias: local provisioning is already the default')
p.add_argument('--default-browser-entitlement', action='store_true', help='Include the managed entitlement only after Apple approves your identifier')
p.add_argument('--bundle-id', default='com.arbiter.mobile', help='iOS reverse-DNS app identifier registered to your team')
p.add_argument('--app-group', help='iOS App Group registered to your team (default: group.<bundle-id>)')
p.add_argument('--build-number', type=int, default=3, help='Monotonic mobile build number; increase for each distributed APK')
p.add_argument('--build-dir', type=Path, help='Override the output directory')
p.add_argument('--release', action='store_true')
p.add_argument('--jobs', type=int, default=min(20, os.cpu_count() or 1))
p.add_argument('--sdk', type=Path, help='Android SDK root')
p.add_argument('--ndk', type=Path, help='Android NDK root')
a = p.parse_args()
if not 1 <= a.build_number <= 2100000000:
    p.error('--build-number must be 1..2100000000')
if not re.fullmatch(r'[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+', a.bundle_id):
    p.error('--bundle-id must be a reverse-DNS identifier')
if a.device_testing and a.default_browser_entitlement:
    p.error('--device-testing conflicts with --default-browser-entitlement')
build = (a.build_dir or root / ('build-mobile-qt-' + a.platform)).expanduser().resolve()
qt = a.qt_kit.expanduser().resolve()
# Equivalent to qt-cmake, without invoking a .bat/shell wrapper on Windows.
toolchain = qt / 'lib/cmake/Qt6/qt.toolchain.cmake'
if not toolchain.is_file():
    p.error('Qt kit has no lib/cmake/Qt6/qt.toolchain.cmake')
command = ['cmake', '-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain),
           '-S', str(root / 'mobile'), '-B', str(build)]
config = 'Release' if a.release else 'Debug'
command += ['-DCMAKE_BUILD_TYPE=' + config, '-DARBITER_MOBILE_BUILD_NUMBER=' + str(a.build_number)]
if a.qt_host:
    command += ['-DQT_HOST_PATH=' + str(a.qt_host.expanduser().resolve())]
if a.platform.startswith('ios'):
    command += ['-G', 'Xcode', '-DCMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY=ON',
                '-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0', '-DCMAKE_OSX_SYSROOT=' + ('iphoneos' if a.platform == 'ios' else 'iphonesimulator')]
    if a.team:
        command += ['-DARBITER_APPLE_TEAM=' + a.team]
    command += ['-DARBITER_IOS_DEVICE_TESTING=' + ('OFF' if a.default_browser_entitlement else 'ON'),
                '-DARBITER_IOS_BUNDLE_ID=' + a.bundle_id,
                '-DARBITER_IOS_APP_GROUP=' + (a.app_group or 'group.' + a.bundle_id)]
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
if a.platform == 'android':
    command += ['--target', 'apk']
if a.platform == 'ios':
    command += ['--', '-allowProvisioningUpdates', '-allowProvisioningDeviceRegistration']
elif a.platform == 'ios-simulator':
    command += ['--', 'CODE_SIGNING_ALLOWED=NO']
subprocess.run(command, check=True)
print('Build output:', build)
