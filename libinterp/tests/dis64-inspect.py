#!/usr/bin/env python3
"""Check the real inspector's opt-in semantic gate and untouched rejection output."""
from pathlib import Path
import json
import os
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Run a literal argv with bounded time and require exactly the expected status.
# The tool reports structure only: no fixture authorizes VM execution.
def call(arguments, good=True):
    result = subprocess.run([str(a) for a in arguments], capture_output=True, text=True, timeout=20)
    if (result.returncode == 0) != good:
        raise RuntimeError(str(arguments) + "\n" + result.stdout + result.stderr)
    return result


# Build the actual upstream CLI in private storage. Mutate in-range semantic
# fields without breaking the envelope, then ensure --pilot rejects before I/O.
def main():
    golden = bytes.fromhex((ROOT / "libinterp/tests/dis64/pilot42.hex").read_text())
    with tempfile.TemporaryDirectory(prefix="dis64 inspect ") as directory:
        work = Path(directory)
        binary, image, output = (work / n for n in ("inspect", "pilot.dis64", "copy.dis64"))
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -Wall -Wextra -Wpedantic -Werror"))
        command += ["-std=c99", "-Wno-unused-function", "-I" + str(ROOT / "include"),
                    str(ROOT / "utils/dis64inspect/dis64inspect.c"), "-o", str(binary)]
        call(command)
        image.write_bytes(golden)
        plain = json.loads(call([binary, image]).stdout)
        report = json.loads(call([binary, image, "--pilot", "--rewrite", output]).stdout)
        if not plain["envelope_only"] or report["envelope_only"] or report["execution_supported"]:
            raise RuntimeError("semantic check misreported execution boundary")
        expected = dict(scalar_pilot=True, data_bytes=4, frame_bytes=64, entry_pc=0,
                        instruction_count=2, string_count=1, type_count=2, initializer_count=0)
        if any(report.get(k) != v for k, v in expected.items()) or output.read_bytes() != golden:
            raise RuntimeError("semantic report or rewrite differs from hand-written golden")
        initialized = bytes.fromhex((ROOT / "libinterp/tests/dis64/pilot-init42.hex").read_text())
        image.write_bytes(initialized)
        report_init = json.loads(call([binary, image, "--pilot", "--rewrite", output]).stdout)
        if report_init.get("initializer_count") != 1 or output.read_bytes() != initialized:
            raise RuntimeError("initialized pilot report/rewrite differs")
        sections = {s["kind"]: s["offset"] for s in plain["sections"]}
        mutations = [(sections[1], 1), (sections[1] + 16, 2),
                     (sections[1] + 20, 2), (sections[4], 1), (sections[3] + 8, 3)]
        for offset, value in mutations:
            bad = bytearray(golden)
            struct.pack_into(">I", bad, offset, value)
            image.write_bytes(bad)
            call([binary, image])  # Same bytes remain an accepted opaque envelope.
            output.write_bytes(b"KEEP")
            call([binary, image, "--pilot", "--rewrite", output], False)
            if output.read_bytes() != b"KEEP":
                raise RuntimeError("semantic rejection changed existing output")
        image.write_bytes(golden)
        for flags in (("--unknown",), ("--pilot", "--pilot"), ("--rewrite",),
                      ("--rewrite", output, "--pilot")):
            call([binary, image, *flags], False)
    print("PASS Dis64 inspector: real pilot metadata, opaque default, semantic rejection and rewrite")


if __name__ == "__main__":
    main()
