#!/usr/bin/env python3
"""Compatibility entry for the ARM64 runtime gate; see native64-runtime.py."""
from pathlib import Path
import runpy


# Preserve existing ARM64 commands while sharing target-independent owning gates.
if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).with_name('native64-runtime.py')), run_name='__main__')
