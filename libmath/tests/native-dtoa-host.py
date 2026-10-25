#!/usr/bin/env python3
"""Compile real dtoa/g_fmt with a minimal host header and independent binary64 oracles."""

import json
import math
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning Inferno source root.


# Run the actual production parser and formatter, not a translated arithmetic model.
# Optional DTOA_SOURCE selects a previous source for replay; CC/CFLAGS choose the host compiler.
# Return normally only after every exact parser/formatter comparison passes under the selected checks.
def main():
    source = Path(os.environ.get("DTOA_SOURCE", ROOT / "libmath/dtoa.c")).resolve()
    literals = [
        "1e400",
        "-1e400",
        "1e-400",
        "-0.0",
        "4.9406564584124654e-324",
        "1.7976931348623157e308",
        "1.0000000000000002",
        "2.2250738585072014e-308",
    ]
    randomizer = random.Random(1549)
    while len(literals) < 1032:
        value = struct.unpack(">d", randomizer.getrandbits(64).to_bytes(8, "big"))[0]
        if math.isfinite(value):
            literals.append(repr(value))
    cases = "".join(
        value + " " + struct.pack(">d", float(value)).hex() + "\n" for value in literals
    )
    with tempfile.TemporaryDirectory(prefix="native-dtoa-") as temporary:
        directory = Path(temporary)
        header = """#ifndef NATIVE_DTOA_FIXTURE_LIB9_H
#define NATIVE_DTOA_FIXTURE_LIB9_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define nil NULL
"""
        if sys.byteorder == "little":
            header += "#define __LITTLE_ENDIAN 1\n"
        header += "#endif\n"
        (directory / "lib9.h").write_text(header)
        unit = directory / "native.c"
        unit.write_text(
            '#include "lib9.h"\n#define strtod native_test_strtod\n'
            "#include " + json.dumps(str(source)) + "\n#undef strtod\n"
            "#include " + json.dumps(str(ROOT / "libmath/g_fmt.c")) + "\n"
            "#include " + json.dumps(str(Path(__file__).with_suffix(".c"))) + "\n"
        )
        binary = directory / "native"
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(
            os.environ.get("CFLAGS", "-O1 -g -fsanitize=undefined -fno-sanitize-recover=all")
        )
        subprocess.run(
            command
            + [
                "-std=c99",
                "-fno-strict-aliasing",
                "-I" + str(directory),
                str(unit),
                "-lm",
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], input=cases, text=True, check=True, timeout=30)


if __name__ == "__main__":
    main()
