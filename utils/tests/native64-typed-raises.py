#!/usr/bin/env python3
"""Source startup raise after nil Module LOAD, with real exception-owner cleanup."""
from pathlib import Path
import runpy
if __name__=='__main__':
    runpy.run_path(str(Path(__file__).with_name('native64-typed-command-files.py')))['main'](argv=True,local=True,pair=True,raises=True)
