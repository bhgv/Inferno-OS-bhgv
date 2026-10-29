#!/usr/bin/env python3
"""Check lossless BE Rune32 records independently of native string ordering."""
import ctypes
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = r'''
#include <string.h>
#include "dis-rune.h"


/* Verify that invalid extents preserve the entire output view. */
int readrecord(const unsigned char *data, unsigned int bytes,
    unsigned int index, unsigned int *value)
{
    DisRuneView view, saved;

    memset(&view, 0x5a, sizeof(view));
    memcpy(&saved, &view, sizeof(view));
    if(!disrunecheck(data, bytes, &view))
        return memcmp(&view, &saved, sizeof(view)) == 0 ? 0 : -1;
    return disruneat(&view, index, value) ? 1 : 2;
}


/* Call the same emitter intended for compiler string-pool construction. */
int writerecord(unsigned char *output, unsigned int capacity,
    const unsigned int *values, unsigned int count, unsigned int *bytes)
{
    return disrunewrite(output, capacity, values, count, bytes);
}
'''


# Compare exact wire bytes, guard preservation and every Rune payload bit.
def main():
    keys = [0, 0x7f, 0xff, 0xd800, 0x10ffff, 0x110000, 0x7fffffff,
            0x80000000, 0xffffffff]
    golden = json.loads((ROOT / "libinterp/tests/dis64/rune-golden.json").read_text())
    golden_bytes = len(golden["runes"]).to_bytes(4, "big")
    golden_bytes += b"".join(value.to_bytes(4, "big") for value in golden["runes"])
    if golden_bytes != bytes.fromhex(golden["hex"]):
        raise RuntimeError("inconsistent hand-written Rune golden")
    records = [[], keys] + [[value] for value in keys]
    records += [[(index * 0x9e3779b9) & 0xffffffff for index in range(count)]
                for count in range(65)]
    checks = 0
    with tempfile.TemporaryDirectory(prefix="dis-rune-") as temporary:
        source = Path(temporary) / "rune.c"
        library = Path(temporary) / "rune.so"
        source.write_text(SOURCE)
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        command += ["-shared", "-fPIC", "-I" + str(ROOT / "include"), str(source), "-o", str(library)]
        subprocess.run(command, check=True)
        loaded = ctypes.CDLL(str(library))
        read = loaded.readrecord
        read.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                         ctypes.POINTER(ctypes.c_uint)]
        write = loaded.writerecord
        write.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p,
                          ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        for values in records:
            expected = len(values).to_bytes(4, "big")
            expected += b"".join(value.to_bytes(4, "big") for value in values)
            array = (ctypes.c_uint * len(values))(*values)
            for capacity in range(len(expected) + 2):
                storage = ctypes.create_string_buffer(b"\xa5" * (len(expected) + 8))
                length = ctypes.c_uint(0x12345678)
                ok = write(storage, capacity, array, len(values), ctypes.byref(length))
                valid = capacity >= len(expected)
                wanted = (expected + b"\xa5" * 8 + b"\x00" if valid else
                          b"\xa5" * (len(expected) + 8) + b"\x00")
                if ok != int(valid) or storage.raw != wanted:
                    raise RuntimeError("Rune emitter capacity/guard mismatch")
                if length.value != (len(expected) if valid else 0x12345678):
                    raise RuntimeError("Rune emitter changed length on failure")
                checks += 1
            for payload in [expected] + [expected[:end] for end in range(len(expected))] + [expected + b"\x00"]:
                storage = ctypes.create_string_buffer(payload)
                for index in range(len(values) + 1):
                    output = ctypes.c_uint(0x12345678)
                    result = read(storage, len(payload), index, ctypes.byref(output))
                    valid = payload == expected
                    want = 1 if valid and index < len(values) else 2 if valid else 0
                    if result != want or output.value != (values[index] if want == 1 else 0x12345678):
                        raise RuntimeError("Rune record read/publication mismatch")
                    checks += 1
    print("PASS lossless Rune32:", checks, "BE bytes, capacities, truncations and indexed reads")


if __name__ == "__main__":
    main()
