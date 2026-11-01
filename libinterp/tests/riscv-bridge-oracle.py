#!/usr/bin/env python3
"""Decode production RV64 tails independently without claiming target execution."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile


# Own compiler scratch outside ROOT. Compile the unchanged emitter and active
# independent decoder, then require instruction mutations to fail the decoder.
def main():
    root = Path(__file__).resolve().parents[2]
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", "-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all"))
    with tempfile.TemporaryDirectory(prefix="rv-bridge-oracle-") as directory:
        work = Path(directory)
        source = root / "libinterp/tests/riscv-bridge-oracle.c"
        command = compiler + flags + ["-I" + str(work), "-I" + str(root / "include"), "-I" + str(root / "libinterp")]
        binary = work / "oracle"
        subprocess.run(command + [str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        header = (root / "libinterp/riscv-emit.h").read_text()
        for name, old, new in (
            ("signed-load", "rvi(&buffer, 0x03, 6, Rva2, Rvt0, 48);", "rvi(&buffer, 0x03, 2, Rva2, Rvt0, 48);"),
            ("linked-return", "rvi(&buffer, 0x67, 0, Rvzero, Rvt0, 0);", "rvi(&buffer, 0x67, 0, Rvra, Rvt0, 0);"),
            ("register-input", "rvi(&buffer, 0x13, 0, Rva1, Rvzero, op);", "rvi(&buffer, 0x13, 0, Rva0, Rvzero, op);"),
        ):
            if header.count(old) != 1:
                raise RuntimeError("RV oracle mutation boundary changed: " + name)
            (work / "riscv-emit.h").write_text(header.replace(old, new, 1))
            target = work / name
            subprocess.run(command + [str(source), "-o", str(target)], check=True)
            result = subprocess.run([str(target)], capture_output=True, text=True)
            if result.returncode == 0 or "FAIL RV bridge decode" not in result.stderr:
                raise RuntimeError("RV mutation did not reach the decoder: " + name)
            print("PASS independent RV bridge removal control:", name, flush=True)


if __name__ == "__main__":
    main()
