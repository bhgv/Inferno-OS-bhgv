#!/usr/bin/env python3
"""Build complete core translation units using the unchanged production header."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ("alt", "conv", "dec", "disc", "gc", "heap", "heapaudit", "link",
           "load", "load64", "readmod", "raise", "stack", "validstk", "xec", "string",
           "comp-amd64", "comp-arm64", "comp-riscv64")
PROBE = r'''
#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include <stddef.h>


/* Check actual target scalars and existing frame links, not a mirrored header. */
int
main(void)
{
#ifdef DIS_WORD32
    if(sizeof(WORD) != 4 || sizeof(UWORD) != 4)
        return 1;
#else
    if(sizeof(WORD) != sizeof(long) || sizeof(UWORD) != sizeof(unsigned long))
        return 1;
#endif
    if(sizeof(LONG) != 8 || sizeof(ULONG) != 8 || sizeof(REAL) != 8 ||
       offsetof(Frame, lr) != 0 || offsetof(Frame, fp) != sizeof(void*) ||
       offsetof(Frame, mr) != 2*sizeof(void*) || offsetof(Frame, t) != 3*sizeof(void*) ||
       sizeof(Frame) != 4*sizeof(void*))
        return 2;
    return 0;
}
'''


# Compile both selections independently; this checks objects, not runtime linkage.
def main():
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", "-O1 -g"))
    includes = ["-I" + str(ROOT / path) for path in
                ("Linux/386/include", "emu/port", "include", "libinterp")]
    with tempfile.TemporaryDirectory(prefix="word32-build-") as temporary:
        work = Path(temporary)
        probe = work / "probe.c"
        probe.write_text(PROBE)
        for label, selection in (("legacy", []), ("WORD32", ["-DDIS_WORD32=1"])):
            command = compiler + flags + selection + includes
            for name in SOURCES:
                subprocess.run(command + ["-c", str(ROOT / "libinterp" / (name + ".c")),
                               "-o", str(work / (label + name + ".o"))], check=True)
            binary = work / label
            subprocess.run(command + [str(probe), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            print("PASS", label, len(SOURCES), "complete core objects and native scalar/frame probe",
                  flush=True)
        for value in ("0", "2"):
            result = subprocess.run(compiler + includes + ["-DDIS_WORD32=" + value,
                                    "-fsyntax-only", str(probe)], capture_output=True)
            if result.returncode == 0 or b"DIS_WORD32 must be 1" not in result.stderr:
                raise RuntimeError("invalid scalar selection was not rejected")
        print("PASS invalid scalar selections rejected; no loader/link/VM execution claim")


if __name__ == "__main__":
    main()
