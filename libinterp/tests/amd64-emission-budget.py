#!/usr/bin/env python3
"""Check production AMD64 late fixups and publication under UBSan at O1/O3."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Compile the actual private emitter, optionally from an explicitly selected
# parent directory for reproduction. No snapshot or installed tool is modified.
def main():
    emitter = Path(os.environ.get("BUDGET_EMITTER_DIR", ROOT / "libinterp")).resolve()
    with tempfile.TemporaryDirectory(prefix="amd64-budget-") as temporary:
        for level in ("-O1", "-O3"):
            binary = Path(temporary) / ("budget" + level[1:])
            command = shlex.split(os.environ.get("CC", "cc"))
            command += ["-std=c99", level, "-g", "-fsanitize=undefined", "-fno-sanitize-recover=all"]
            command += ["-I" + str(emitter), "-I" + str(ROOT / "include"), "-I" + str(ROOT / "libinterp")]
            command += [str(ROOT / "libinterp/tests/amd64-emission-budget.c"), "-o", str(binary)]
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
