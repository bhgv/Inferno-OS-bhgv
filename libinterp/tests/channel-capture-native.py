#!/usr/bin/env python3
"""Run real channel allocation with callbacks borrowing decoded operands."""
from pathlib import Path
import runpy
import platform
import sys

if platform.machine().lower() not in ('x86_64', 'amd64'):
    raise SystemExit('channel-capture-native requires an AMD64 host for real RX execution')

sys.argv.append('--captured')
runpy.run_path(str(Path(__file__).with_name('scalar-channel-native.py')), run_name='__main__')
