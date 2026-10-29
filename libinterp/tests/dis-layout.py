#!/usr/bin/env python3
"""Compile target-layout fixtures; this does not enable a runtime Dis64 ABI."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Build the checked C fixtures with caller-selected compiler and sanitizer flags.
def main():
    with tempfile.TemporaryDirectory(prefix="dis-layout-") as temporary:
        binary = Path(temporary) / "layout"
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        command += ["-I" + str(ROOT / "include"),
                    str(ROOT / "libinterp/tests/dis-layout.c"), "-o", str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
