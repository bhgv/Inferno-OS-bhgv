#!/usr/bin/env python3
"""Cross-check scalar Limbo output with an explicitly supplied external Dis64 reader."""
from pathlib import Path
import argparse
import json
import os
import shlex
import signal
import struct
import subprocess
import tempfile


# Run borrowed executable arguments without a shell. Return stdout or report the
# complete diagnostic. Own no child beyond this blocking call; timeout is fatal.
def command(arguments, accepted=True):
    result = subprocess.run([str(value) for value in arguments], capture_output=True,
                            timeout=30)
    if (result.returncode == 0) != accepted:
        raise RuntimeError("Unexpected command result: " + repr(arguments) + "\n" +
                           result.stdout.decode(errors="replace") +
                           result.stderr.decode(errors="replace"))
    return result.stdout


# Generate 64 layouts with live Big and WORD locals around a scratch-using WORD
# expression. The returned source owns its strings; no generated parser is used.
def source(big_count, word_count):
    declarations = ["b%d := big %d;" % (i, 17 + i) for i in range(big_count)]
    declarations += ["w%d := %d;" % (i, 31 + i) for i in range(word_count)]
    checks = ["b%d != big %d" % (i, 17 + i) for i in range(big_count)]
    checks += ["w%d != %d" % (i, 31 + i) for i in range(word_count)]
    checks.append("answer != 78636")
    return ("implement ExternalReader;\nExternalReader: module { init: fn(); };\n"
            "left := 12345; right := 789; answer: int;\ninit() {\n    " +
            "\n    ".join(declarations) +
            "\n    answer = ((left ^ right) & 16rffff) | 16r10000;\n    if (" +
            " || ".join(checks) + ") answer /= 0;\n}\n")


# Locate descriptors using independent envelope arithmetic. Return a private
# corrupt image whose frame map falsely marks the reserved prefix as a root.
def corrupted_map(data):
    image = bytearray(data)
    count = struct.unpack_from(">I", image, 28)[0]
    for index in range(count):
        kind, flags, offset, size = struct.unpack_from(">HHII", image, 32 + 12 * index)
        if kind != 3:
            continue
        descriptors = struct.unpack_from(">I", image, offset)[0]
        if descriptors != 2 or offset + size > len(image):
            raise RuntimeError("Unexpected compiler descriptor geometry")
        cursor = offset + 4
        for ordinal in range(descriptors):
            extent, alignment = struct.unpack_from(">II", image, cursor)
            cursor += 8
            map_bytes = (extent + 63) // 64
            if ordinal == 1:
                if extent < 64 or alignment != 8 or not map_bytes:
                    raise RuntimeError("Missing native64 entry prefix")
                image[cursor] = 128
                return image
            cursor += map_bytes
    raise RuntimeError("Missing types section")


# Execute an explicitly selected experimental whole emulator in a private process
# group. Its clean-exit path may kill that group; require late retirement and
# semantic completion markers rather than treating a diagnostic as success.
def execute(emu, root, runner, image, jit, error):
    arguments = shlex.split(runner) + [str(emu), "-v", "-r", str(root), "-c",
                                      str(jit), "-d", "/" + str(image.relative_to(root))]
    process = subprocess.Popen(arguments, start_new_session=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        output, unused = process.communicate(timeout=30)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate()
        raise RuntimeError("Whole emulator did not retire the scalar program")
    marker = 'native64 scalar module ExternalReader: jit=' + str(jit) + ', exit="' + error + '"'
    if (process.returncode not in (0, -signal.SIGKILL) or marker not in output or
            "native64 scalar scheduler empty after program retirement" not in output or
            ("Broken:" in output) != bool(error) or "panic:" in output or "faults:" in output):
        raise RuntimeError("Whole emulator semantic/retirement failure:\n" + output)


# Own a scratch directory; borrow explicit compiler/reader executables. Compare
# actual products and reader reports. This proves interoperability, not VM boot,
# physical target execution or managed/native-call admission. Optional whole
# emulator execution additionally checks selected JIT modes and real retirement.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--limbo", required=True, type=Path)
    parser.add_argument("--reader", required=True, type=Path)
    parser.add_argument("--other-limbo", type=Path)
    parser.add_argument("--root", type=Path, help="explicit experimental emulator root")
    parser.add_argument("--emu", type=Path, help="optional whole-emulator execution")
    parser.add_argument("--runner", default="", help="optional explicit target executor")
    args = parser.parse_args()
    if bool(args.root) != bool(args.emu):
        parser.error("--root and --emu must be supplied together")
    root = args.root.resolve() if args.root else None
    emu = args.emu.resolve() if args.emu else None
    compiler = args.limbo.resolve()
    reader = args.reader.resolve()
    other = args.other_limbo.resolve() if args.other_limbo else None
    with tempfile.TemporaryDirectory(prefix="dis64-cross-reader-",
                                     dir=root or os.environ.get("TMPDIR")) as directory:
        work = Path(directory)
        input_file = work / "layout.b"
        image = work / "layout.dis64"
        rewritten = work / "rewrite.dis64"
        alternate = work / "alternate.dis64"
        legacy = work / "legacy.dis"
        for big_count in range(8):
            for word_count in range(8):
                input_file.write_text(source(big_count, word_count), encoding="utf-8")
                command([compiler, "-o", legacy, input_file])
                ordinary = legacy.read_bytes()
                command([compiler, "-64", "-o", image, input_file])
                encoded = image.read_bytes()
                if other:
                    command([other, "-6", "-o", alternate, input_file])
                    if alternate.read_bytes() != encoded:
                        raise RuntimeError("Compiler byte mismatch at layout " +
                                           repr((big_count, word_count)))
                report = json.loads(command([reader, image, "--pilot", "--rewrite", rewritten]))
                if (report["execution_supported"] or not report["scalar_pilot"] or
                        report["entry_pc"] != 0 or report["frame_bytes"] < 80 or
                        report["instruction_count"] < 2 or rewritten.read_bytes() != encoded):
                    raise RuntimeError("External reader contract changed: " + repr(report))
                command([compiler, "-o", legacy, input_file])
                if legacy.read_bytes() != ordinary:
                    raise RuntimeError("Scalar compilation changed ordinary Dis output")
                if emu:
                    for jit in (0, 1):
                        execute(emu, root, args.runner, image, jit, "")
                image.write_bytes(corrupted_map(encoded))
                rewritten.write_bytes(b"KEEP")
                command([reader, image, "--pilot", "--rewrite", rewritten], False)
                if rewritten.read_bytes() != b"KEEP":
                    raise RuntimeError("Rejected frame map overwrote rewrite destination")
        if emu:
            input_file.write_text(source(7, 7).replace("answer != 78636", "answer != 78637"),
                                  encoding="utf-8")
            command([compiler, "-64", "-o", image, input_file])
            for jit in (0, 1):
                execute(emu, root, args.runner, image, jit, "zero divide")
    if emu:
        print("PASS complete emulator: 128 live-local processes and two active wrong-answer controls")
    print("PASS external Dis64 reader: 64 live-local layouts, exact rewrites, "
          "ordinary Dis identity, 64 managed-map refusals" +
          (", independent compiler byte identity" if other else ""))


if __name__ == "__main__":
    main()
