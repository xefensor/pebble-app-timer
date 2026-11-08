#!/usr/bin/env python3
"""Build Timer seed/probe helpers for isolated emulator QA only.

These helpers share Timer's UUID and can replace its saved state. Never install
one on a physical watch. Build the app candidate first: its clean removes build/.
"""
from pathlib import Path
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
fixture = root / 'tests/ux-fixture'
for mode in (0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12):
    environment = {**os.environ, 'UX_FIXTURE_MODE': str(mode)}
    subprocess.run(['pebble', 'clean'], cwd=fixture, env=environment, check=True)
    subprocess.run(['pebble', 'build'], cwd=fixture, env=environment, check=True)
    shutil.copyfile(fixture / 'build/ux-fixture.pbw', root / ('build/fixture-mode' + str(mode) + '.pbw'))
print('Built twelve emulator-only Timer seed/probe helpers')
