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

/* Nil is an explicit fixture argument, never guessed from a record's bytes. */
int comparestrings(const unsigned char *a, unsigned int na,
    const unsigned char *b, unsigned int nb, int *order)
{
    DisRuneView left, right;

    if(a != 0 && !disrunecheck(a, na, &left))
        return 0;
    if(b != 0 && !disrunecheck(b, nb, &right))
        return 0;
    return disrunecompare(a == 0 ? 0 : &left, b == 0 ? 0 : &right, order);
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
        compare = loaded.comparestrings
        compare.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p,
                            ctypes.c_uint, ctypes.POINTER(ctypes.c_int)]
        comparisons = 0
        order_fixture = json.loads((ROOT / "libinterp/tests/dis64/rune-order-golden.json").read_text())
        groups = order_fixture["equal_sorted_groups"]
        contents = order_fixture["strings"] + [None] + records + [[key, tail] for key in keys for tail in keys]
        ranks = {}
        for rank, group in enumerate(groups):
            for index in group:
                ranks[index] = rank
        for left_index, left in enumerate(order_fixture["strings"]):
            for right_index, right in enumerate(order_fixture["strings"]):
                a = [value if value < 2**31 else value - 2**32 for value in (left or [])]
                b = [value if value < 2**31 else value - 2**32 for value in (right or [])]
                if ((a > b) - (a < b)) != ((ranks[left_index] > ranks[right_index]) -
                                                         (ranks[left_index] < ranks[right_index])):
                    raise RuntimeError("inconsistent hand-written Rune order golden")
        for left in contents:
            for right in contents:
                payloads = []
                expected_keys = []
                for values in (left, right):
                    if values is None:
                        payloads.append(None)
                        expected_keys.append([])
                    else:
                        image = len(values).to_bytes(4, "big")
                        image += b"".join(value.to_bytes(4, "big") for value in values)
                        payloads.append(ctypes.create_string_buffer(image))
                        expected_keys.append([value if value < 2**31 else value - 2**32
                                              for value in values])
                expected_order = ((expected_keys[0] > expected_keys[1]) -
                                  (expected_keys[0] < expected_keys[1]))
                order = ctypes.c_int(17)
                ok = compare(payloads[0], 0 if payloads[0] is None else len(payloads[0].raw) - 1,
                             payloads[1], 0 if payloads[1] is None else len(payloads[1].raw) - 1,
                             ctypes.byref(order))
                if ok != 1 or order.value != expected_order:
                    raise RuntimeError("signed32 Rune comparison mismatch")
                comparisons += 1
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
    print("PASS lossless Rune32:", checks, "BE bytes, capacities, truncations and indexed reads;", comparisons,
          "signed32 string comparisons")


if __name__ == "__main__":
    main()
