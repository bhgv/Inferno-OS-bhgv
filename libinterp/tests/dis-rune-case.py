#!/usr/bin/env python3
"""Combine real Rune32 decoding/comparison with typed case validation and lookup."""
import ctypes
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = r'''
#include "dis-rune.h"
#include "dis-control.h"

/* Fixture-owned pool index; its array position does not assign a wire nil ID. */
typedef struct Pool Pool;
struct Pool {
    DisRuneView views[64]; // Decoded non-nil records owned by the test.
    unsigned int count; // Available fixture IDs.
};


/* Resolve fixture IDs and use the actual shared signed32 comparison helper. */
static int
compare(void *context, unsigned int a, unsigned int b, int *order)
{
    Pool *pool = context;

    if(a >= pool->count || b >= pool->count)
        return 0;
    return disrunecompare(&pool->views[a], &pool->views[b], order);
}


/* Build checked borrowed views before validating or selecting any table target. */
int
query(const unsigned char *table, unsigned int bytes,
    const unsigned char **records, const unsigned int *lengths,
    unsigned int count, unsigned int id, unsigned int *target)
{
    Pool pool;
    DisControlView view;
    unsigned char key[4];
    unsigned int i;

    if(count > 64)
        return 0;
    pool.count = count;
    for(i=0; i<count; i++)
        if(!disrunecheck(records[i], lengths[i], &pool.views[i]))
            return 0;
    if(!discontrolcheck(DisControlString, table, bytes, 8, compare, &pool, &view))
        return 0;
    disruneput(key, id);
    return discontrolcase(DisControlString, &view, key, compare, &pool, target);
}
'''


# Interpret Rune32 mathematically, independently of the C sign-flip implementation.
def signed_key(values):
    return tuple(value if value < 2**31 else value - 2**32 for value in values)


# Check identical ordering across non-monotone pool IDs, range endpoints and search.
def main():
    keys = [0, 0x7f, 0xff, 0xd800, 0x10ffff, 0x110000, 0x7fffffff,
            0x80000000, 0xffffffff]
    strings = [[], [0x80000000, 0xffffffff]] + [[key] for key in keys]
    strings += [[0, key] for key in keys] + [[key, 0] for key in keys]
    images = [len(values).to_bytes(4, "big") + b"".join(value.to_bytes(4, "big")
                                                        for value in values)
              for values in strings]
    ordered = sorted(range(len(strings)), key=lambda index: signed_key(strings[index]))
    unique = []
    for index in ordered:
        if not unique or strings[unique[-1]] != strings[index]:
            unique.append(index)
    storage = [ctypes.create_string_buffer(image) for image in images]
    pointers = (ctypes.c_void_p * len(images))(*(ctypes.addressof(item) for item in storage))
    lengths = (ctypes.c_uint * len(images))(*(len(image) for image in images))
    cases = 0
    with tempfile.TemporaryDirectory(prefix="dis-rune-case-") as temporary:
        source = Path(temporary) / "case.c"
        library = Path(temporary) / "case.so"
        source.write_text(SOURCE)
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        command += ["-Wno-unused-function", "-shared", "-fPIC", "-I" + str(ROOT / "include"),
                    str(source), "-o", str(library)]
        subprocess.run(command, check=True)
        query = ctypes.CDLL(str(library)).query
        query.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_void_p,
                          ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        for first in range(len(unique)):
            for last in range(first, len(unique)):
                rows = [(unique[first], unique[last], 3)]
                if first > 0:
                    rows.insert(0, (unique[0], unique[first - 1], 1))
                if last + 1 < len(unique):
                    rows.append((unique[last + 1], unique[-1], 5))
                payload = len(rows).to_bytes(4, "big")
                payload += b"".join(value.to_bytes(4, "big") for row in rows for value in row)
                payload += (7).to_bytes(4, "big")
                table = ctypes.create_string_buffer(payload)
                for index in range(len(strings) + 1):
                    output = ctypes.c_uint(0x12345678)
                    ok = query(table, len(payload), pointers, lengths, len(strings),
                               index, ctypes.byref(output))
                    wanted = 7
                    if index < len(strings):
                        key = signed_key(strings[index])
                        for lo, hi, target in rows:
                            if signed_key(strings[lo]) <= key <= signed_key(strings[hi]):
                                wanted = target
                                break
                    if ok != int(index < len(strings)) or output.value != (
                            wanted if index < len(strings) else 0x12345678):
                        raise RuntimeError("wire Rune/string-case integration mismatch")
                    cases += 1
    print("PASS Rune32 -> typed string case:", cases, "range/ID/lookup combinations")


if __name__ == "__main__":
    main()
