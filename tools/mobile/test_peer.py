#!/usr/bin/env python3
"""Exercise the phone client against a temporary native peer containing dummy data."""
import argparse
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--device', required=True)
p.add_argument('--fixture', type=pathlib.Path, required=True)
p.add_argument('--flutter', type=pathlib.Path, default=root/'build-mobile-tools/flutter/bin/flutter')
a=p.parse_args()
peer=subprocess.Popen([str(a.fixture.resolve())],stdout=subprocess.PIPE,text=True)
try:
    code=peer.stdout.readline().strip()
    if not code.startswith('arbiter-pair://v1?data='):
        raise RuntimeError('The temporary native peer did not produce a pairing invitation')
    result=subprocess.run([str(a.flutter.resolve()),'--suppress-analytics','test',
        'integration_test/credential_peer_test.dart','-d',a.device,'--dart-define=ARBITER_TEST_PAIRING_CODE='+code],cwd=root/'mobile')
    sys.exit(result.returncode)
finally:
    peer.terminate()
    try:peer.wait(timeout=5)
    except subprocess.TimeoutExpired:peer.kill();peer.wait()
