#!/usr/bin/env python3
"""Run raw/mapped channel C/RX oracles with the shared real allocation fixture."""
from pathlib import Path
import runpy
import sys

sys.argv.append('--payload')
runpy.run_path(str(Path(__file__).with_name('scalar-channel-native.py')), run_name='__main__')
