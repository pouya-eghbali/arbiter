#!/usr/bin/env python3
"""Provision base Qt; WebEngine and Qt PDF are built together in the engine SDK."""
import json
from pathlib import Path
import subprocess

manifest = json.loads(Path('/opt/arbiter-pins/manifest.json').read_text())
subprocess.run(['aqt', 'install-qt', 'linux', 'desktop', manifest['qt'], 'linux_gcc_64',
                '-O', '/opt/qt', '-m', 'qtwebchannel', 'qtpositioning'], check=True)
