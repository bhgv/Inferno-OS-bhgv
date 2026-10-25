#!/usr/bin/env python3
"""Run production string/UTF routines with compiler-local byte descriptors.

Only fixture inputs are derived in memory. Production C is read unchanged by the
existing UBSan runners, in both WORD32 and native WORD models.
"""

from pathlib import Path
import runpy
from unittest.mock import patch


# Run existing alignment/alias/error matrices with an equivalent local descriptor.
# No arguments or result; compiler/runtime failures propagate as nonzero exit.
def main():
    root = Path(__file__).resolve().parents[2]
    original = Path.read_text

    # Borrow fixture text and return a derived copy; never alter production files.
    def read(path, *args, **kwargs):
        text = original(path, *args, **kwargs)
        if path == root / "libinterp/tests/string-slots.c":
            text = text.replace("Array *a;", "Array *a; Type modulebyte = {1, 0, 0, 1};", 1)
            anchor = "memmove(before,slots.bytes,128);cvtac();"
            if text.count(anchor) != 1:
                raise ValueError("string conversion fixture boundary changed")
            text = text.replace(anchor, "a->t=&modulebyte;" + anchor)
        if path == root / "libinterp/tests/string-codecs.c":
            text = text.replace(
                "((Array*)v)->t == &Tbyte",
                "(((Array*)v)->t == &Tbyte || (((Array*)v)->t->size == 1 && ((Array*)v)->t->np == 0))",
            )
        if path == root / "libinterp/tests/sys-utf.c":
            text = text.replace(
                "Type Tarray = {1}, Tbyte = {1}, other = {1};",
                "Type Tarray = {1}, Tbyte = {1}, other = {1}, modulebyte = {1, 0, 0, 1};",
            )
            text = text.replace("a->t = &Tbyte;", "a->t = &modulebyte;")
        return text

    with patch.object(Path, "read_text", read):
        for runner in ("string-slots.py", "sys-utf.py"):
            runpy.run_path(str(root / "libinterp/tests" / runner), run_name="__main__")
    print("PASS module-local byte descriptors: string/UTF, WORD32/native, UBSan")


if __name__ == "__main__":
    main()
