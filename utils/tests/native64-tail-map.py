#!/usr/bin/env python3
"""Run an independent byte/slot boundary oracle for the private typed-tail map."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Tools/flags are explicit environment values. Only copied metadata is tested;
# actual collector lifetime evidence remains in native64-tail.py/-gc.c.
def main():
    flags = shlex.split(os.environ.get(
        "NATIVE64_CFLAGS", "-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all"))
    compiler = shlex.split(os.environ.get("CC", "cc"))
    includes = ["-I" + str(ROOT / path) for path in
                ("Linux/amd64/include", "Linux/386/include", "emu/port", "include", "libinterp")]
    with tempfile.TemporaryDirectory(prefix="typed-tail-boundaries-", dir=ROOT) as directory:
        work = Path(directory)
        for name, model in (("WORD32", ["-DDIS_WORD32=1"]),
                            ("WORD32-file", ["-DDIS_WORD32=1", "-DDIS64_PILOT_LOADER=1"])):
            binary = work / name
            subprocess.run(compiler + flags + model + includes +
                           [str(ROOT / "utils/tests/native64-tail-map.c"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            negative = subprocess.run([str(binary), "damage"], text=True, capture_output=True)
            if negative.returncode != 1 or "root/padding bit mismatch" not in negative.stderr:
                raise RuntimeError("map corruption did not reach the active independent oracle: " + negative.stderr)
            print("PASS typed-tail boundary corruption control:", name, flush=True)


if __name__ == "__main__":
    main()
