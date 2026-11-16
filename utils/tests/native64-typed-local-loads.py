#!/usr/bin/env python3
"""Return owned loaded Modules and argv through private checked Limbo helpers."""
from pathlib import Path
import runpy
if __name__=='__main__':
    runpy.run_path(str(Path(__file__).with_name('native64-typed-command-files.py')))['main'](argv=True,local=True)
