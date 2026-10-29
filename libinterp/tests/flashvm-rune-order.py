"""Independently compare selected Inferno Rune views with signed Python lists."""

from pathlib import Path
import ctypes
import os
import shlex
import subprocess
import sys
import tempfile

ADAPTER = r"""
#include "dis-rune.h"


/* Validate borrowed complete records, then call the selected view-level helper.
 * Output belongs to the caller and is unchanged when either record is invalid.
 * Inputs remain immutable, unaligned spans are allowed, and no VM is started. */
int compare_records(const unsigned char *left, unsigned int leftbytes,
                    const unsigned char *right, unsigned int rightbytes, int *order)
{
    DisRuneView a, b;
    if(!disrunecheck(left, leftbytes, &a) || !disrunecheck(right, rightbytes, &b)) {
        return 0;
    }
    return disrunecompare(&a, &b, order);
}


/* Exercise the selected writer and indexed-reader contracts in one scalar call.
 * The caller owns all buffers/results; failures publish no successful roundtrip. */
int roundtrip(unsigned char *output, unsigned int capacity,
              const unsigned int *values, unsigned int count, unsigned int *last)
{
    DisRuneView view;
    unsigned int bytes;
    if(!disrunewrite(output, capacity, values, count, &bytes) ||
       !disrunecheck(output, bytes, &view)) {
        return 0;
    }
    return count == 0 || disruneat(&view, count-1, last);
}
"""


# Build exact wire records with explicit byte order, independent of C struct ABI.
def record(values):
    return len(values).to_bytes(4, "big") + b"".join(
        v.to_bytes(4, "big") for v in values
    )


# Interpret payload bits mathematically; Unicode/host casts never define order.
def signed_values(values):
    return [v if v < 0x80000000 else v - 0x100000000 for v in values]


# CLI supplies the selected source root. Compile a small adapter against its
# actual header; own all temporary files and ctypes buffers for the whole call.
# No bootstrap, target runtime, network, GC or pointer transport is involved.
def main():
    upstream = Path(sys.argv[1]).resolve()
    keys = [
        0,
        0x7F,
        0xFF,
        0xD800,
        0x10FFFF,
        0x110000,
        0x7FFFFFFF,
        0x80000000,
        0xFFFFFFFF,
    ]
    records = [[], keys] + [[v] for v in keys]
    records += [[(i * 0x9E3779B9) & 0xFFFFFFFF for i in range(n)] for n in range(65)]
    checks = 0
    with tempfile.TemporaryDirectory(prefix="Rune order crosscheck ") as directory:
        work = Path(directory)
        source, binary = work / "adapter.c", work / "adapter.so"
        source.write_text(ADAPTER)
        flags = shlex.split(os.environ.get("CFLAGS", "-O2 -Wall -Wextra -Werror"))
        command = shlex.split(os.environ.get("CC", "cc"))
        subprocess.run(
            command
            + flags
            + [
                "-std=c99",
                "-shared",
                "-fPIC",
                "-I" + str(upstream / "include"),
                str(source),
                "-o",
                str(binary),
            ],
            check=True,
        )
        library = ctypes.CDLL(str(binary))
        compare = library.compare_records
        compare.argtypes = [
            ctypes.c_void_p,
            ctypes.c_uint,
            ctypes.c_void_p,
            ctypes.c_uint,
            ctypes.POINTER(ctypes.c_int),
        ]
        roundtrip = library.roundtrip
        roundtrip.argtypes = [
            ctypes.c_void_p,
            ctypes.c_uint,
            ctypes.c_void_p,
            ctypes.c_uint,
            ctypes.POINTER(ctypes.c_uint),
        ]
        for left in records:
            a = record(left)
            ordered_left = signed_values(left)
            storage = ctypes.create_string_buffer(len(a))
            values = (ctypes.c_uint * len(left))(*left)
            last = ctypes.c_uint(12345)
            if not roundtrip(storage, len(a), values, len(left), ctypes.byref(last)):
                raise RuntimeError("selected Rune record roundtrip failed")
            if storage.raw != a or (left and last.value != left[-1]):
                raise RuntimeError("selected Rune writer changed payload bits")
            checks += 1
            for right in records:
                b = record(right)
                ordered_right = signed_values(right)
                expected = (ordered_left > ordered_right) - (
                    ordered_left < ordered_right
                )
                order = ctypes.c_int(12345)
                if (
                    compare(a, len(a), b, len(b), ctypes.byref(order)) != 1
                    or order.value != expected
                ):
                    raise RuntimeError(
                        "selected Rune comparison differs from independent signed oracle"
                    )
                checks += 1
                for bad_a, bad_b in (
                    (a[:-1], b),
                    (a, b[:-1]),
                    (a + b"\0", b),
                    (a, b + b"\0"),
                ):
                    order.value = 12345
                    if (
                        compare(
                            bad_a, len(bad_a), bad_b, len(bad_b), ctypes.byref(order)
                        )
                        or order.value != 12345
                    ):
                        raise RuntimeError("invalid Rune extent published an order")
                    checks += 1
            shifted = ctypes.create_string_buffer(b"!" + a)
            alias = ctypes.cast(ctypes.byref(shifted, 1), ctypes.c_void_p)
            order = ctypes.c_int(12345)
            if (
                compare(alias, len(a), alias, len(a), ctypes.byref(order)) != 1
                or order.value != 0
            ):
                raise RuntimeError("unaligned identical Rune views compare unequal")
            checks += 1
    print(
        "PASS independent signed Rune32 crosscheck:",
        checks,
        "selected-header/oracle checks",
    )


if __name__ == "__main__":
    main()
