#!/usr/bin/env python3
"""Source String/List primitives composed with checked LOAD and real file calls."""
from pathlib import Path
import runpy
if __name__=='__main__':
    runpy.run_path(str(Path(__file__).with_name('native64-typed-managed-files.py')))['main'](lists=True)
