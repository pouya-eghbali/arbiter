#!/usr/bin/env python3
"""Run native crypto vectors using fabricated data, without opening the OS vault."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
build=root/'build-mobile-native-crypto'
build.mkdir(exist_ok=True)
subprocess.run(['xcrun','swiftc','-parse-as-library',str(root/'mobile/platform/ios/CredentialVaultShared.swift'),str(root/'mobile/tests/native_crypto.swift'),'-o',str(build/'native_crypto')],check=True)
subprocess.run([str(build/'native_crypto')],check=True)
