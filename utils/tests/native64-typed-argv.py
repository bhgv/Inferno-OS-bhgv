#!/usr/bin/env python3
"""Construct startup argv in checked Limbo and invoke a file Command/Wm.init."""
from pathlib import Path
import runpy
if __name__=='__main__':
    runpy.run_path(str(Path(__file__).with_name('native64-typed-command-files.py')))['main'](argv=True)
