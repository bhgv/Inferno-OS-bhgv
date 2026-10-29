#!/usr/bin/env python3
"""Validate the real pilot bytes and execute production scalar C/AMD64 RX paths."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract the actual checked MOVW body, never a mirrored implementation.
def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:brace + token.end()]
    raise RuntimeError("missing production function end")


# Model WORD32/ref64 explicitly; this is still a harness, not a full ABI runtime.
def main():
    if platform.machine().lower() not in ("x86_64", "amd64"):
        print("SKIP pilot RX fixture: requires AMD64 host")
        return
    with tempfile.TemporaryDirectory(prefix="dis64-pilot-native-") as temporary:
        work = Path(temporary)
        header = (ROOT / "include/interp.h").read_text()
        header, words = re.subn(r"typedef long\s+WORD;", "typedef int WORD;", header, count=1)
        header, uwords = re.subn(r"typedef unsigned long\s+UWORD;", "typedef unsigned int UWORD;", header, count=1)
        if words != 1 or uwords != 1:
            raise RuntimeError("WORD model header changed")
        (work / "interp.h").write_text(header)
        implementation = function((ROOT / "libinterp/xec.c").read_text(), "OP(movw)")
        unit = (ROOT / "libinterp/tests/dis64-pilot-native.c").read_text()
        source = work / "pilot.c"
        source.write_text(unit.replace("MOVW_IMPLEMENTATION", implementation))
        image = work / "pilot42.dis64"
        image.write_bytes(bytes.fromhex((ROOT / "libinterp/tests/dis64/pilot42.hex").read_text()))
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O1 -g -fsanitize=undefined -fno-sanitize-recover=all"))
        command += ["-I" + str(work)]
        command += ["-I" + str(ROOT / directory) for directory in
                    ("Linux/arm/include", "emu/port", "include", "libinterp")]
        binary = work / "pilot"
        subprocess.run(command + [str(source), "-o", str(binary), "-lm"], check=True)
        subprocess.run([str(binary), str(image)], check=True)


if __name__ == "__main__":
    main()
