#!/usr/bin/env python3
"""Validate a release tag before any trusted native runner is used."""
import os
from pathlib import Path
import re
import subprocess


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


def validate(tag, branch):
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:-[A-Za-z0-9]+(?:\.[A-Za-z0-9]+)*)?', tag):
        raise ValueError('Expected a release tag such as v0.1.0 or v0.1.0-rc.1')
    sha = git('rev-parse', '--verify', f'refs/tags/{tag}^{{commit}}')
    subprocess.run(['git', 'merge-base', '--is-ancestor', sha, f'origin/{branch}'], check=True)
    if git('show', f'{sha}:VERSION.txt') != tag[1:]:
        raise ValueError('The tag must match VERSION.txt at the tagged commit')
    return {'sha': sha, 'tag': tag, 'prerelease': str('-' in tag).lower()}


if __name__ == '__main__':
    values = validate(os.environ['TAG'], os.environ['DEFAULT_BRANCH'])
    with Path(os.environ['GITHUB_OUTPUT']).open('a') as out:
        for key, value in values.items(): out.write(f'{key}={value}\n')
