#!/usr/bin/env python3
"""Include the Dis-to-C translator and runtime integration in the VM regression set."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, str(root/'utils/dis2c/tests/test.py')], check=True)
