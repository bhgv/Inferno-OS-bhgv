#!/usr/bin/env python3
"""Compile the real caller header against an independent bounded transition oracle."""
from pathlib import Path
import os
import platform
import shlex
import subprocess
import tempfile


# Compile actual header predicates for both compatible WORD32 profiles and require
# four contract removals to fail semantic checks, not merely crash under UBSan.
# Explicit compiler/flags/scratch inputs are borrowed; no emulator or file executes.
def main():
    root = Path(__file__).resolve().parents[2]
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get(
        "CFLAGS", "-O3 -g -DNDEBUG -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all"
    ))
    arch = os.environ.get("NATIVE64_ARCH", {
        "x86_64": "amd64", "amd64": "amd64", "aarch64": "arm64",
        "arm64": "arm64", "riscv64": "riscv64",
    }.get(platform.machine().lower(), ""))
    if platform.system() != "Linux" or arch not in ("amd64", "arm64", "riscv64"):
        raise SystemExit("caller sequence fixture requires native Linux64 headers")
    header = (root / "include/native64-caller.h").read_text()
    controls = {
        "epoch": ("prog->native64epoch == caller->epoch && ", ""),
        "self": ("caller->self != caller || caller->prog == nil", "caller->self == nil || caller->prog == nil"),
        "hold": ("prog->native64holds = 1;", "/* Removed hold control. */"),
        "rebind": ("\n\t\tR.FP = candidate.R.FP;", "\n\t\t(void)caller; /* Removed live transfer. */"),
    }
    with tempfile.TemporaryDirectory(prefix="caller-sequences-") as directory:
        work = Path(directory)
        for control in [None, *controls]:
            source = header
            if control:
                old, new = controls[control]
                if source.count(old) != 1:
                    raise RuntimeError("control boundary changed: " + control)
                source = source.replace(old, new, 1)
            (work / "native64-caller.h").write_text(source)
            for filemode in (False, True):
                binary = work / "oracle"
                command = compiler + flags + ["-std=c99", "-Wno-unused-function", "-Wno-sign-compare", "-Wno-unknown-pragmas",
                    "-DDIS_WORD32=1", "-DDIS_NATIVE64_FIXED_MATH_CALL=1", "-DDIS_NATIVE64_CALLER_GUARD=1"]
                if filemode:
                    command += ["-DDIS64_PILOT_LOADER=1"]
                command += ["-I" + str(work)]
                command += ["-I" + str(root / part) for part in
                            ("Linux/" + arch + "/include", "Linux/386/include", "emu/port", "include", "libinterp")]
                command += [str(root / "libinterp/tests/native64-caller-sequences.c"), "-o", str(binary)]
                subprocess.run(command, check=True)
                result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
                if control:
                    if result.returncode != 1 or "FAIL caller sequence:" not in result.stderr:
                        raise RuntimeError("semantic control missed: " + control + result.stderr)
                    print("PASS caller sequence control:", control, "file" if filemode else "ordinary")
                else:
                    if result.returncode:
                        raise RuntimeError(result.stdout + result.stderr)
                    print(result.stdout, end="")


if __name__ == "__main__":
    main()
