#!/usr/bin/env python3
"""Compatibility entry point: run the shared production GUI I/O regression."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("guiread.py")),run_name="__main__")
