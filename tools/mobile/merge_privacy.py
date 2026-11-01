#!/usr/bin/env python3
"""Merge app + statically linked SDK API declarations; never silently add tracking."""
import argparse
from pathlib import Path
import plistlib


def merge(paths):
    reasons = {}
    for path in paths:
        data = plistlib.loads(Path(path).read_bytes())
        if data.get('NSPrivacyTracking') or data.get('NSPrivacyTrackingDomains') or data.get('NSPrivacyCollectedDataTypes'):
            raise ValueError(f'{path}: SDK data practices changed; review the policy before building')
        for entry in data.get('NSPrivacyAccessedAPITypes', []):
            reasons.setdefault(entry['NSPrivacyAccessedAPIType'], set()).update(entry['NSPrivacyAccessedAPITypeReasons'])
    return dict(NSPrivacyTracking=False, NSPrivacyTrackingDomains=[], NSPrivacyCollectedDataTypes=[],
                NSPrivacyAccessedAPITypes=[dict(NSPrivacyAccessedAPIType=key, NSPrivacyAccessedAPITypeReasons=sorted(values))
                                          for key, values in sorted(reasons.items())])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('inputs', nargs='+', type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(plistlib.dumps(merge(args.inputs)))
