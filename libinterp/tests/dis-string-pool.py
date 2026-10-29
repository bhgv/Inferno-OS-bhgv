#!/usr/bin/env python3
"""Independently verify canonical Dis64 pool bytes, IDs, signed order and failures."""
import ctypes
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ADAPTER = r'''
#include <string.h>
#include "dis-string-pool.h"


/* Check atomic reader/getter publication and expose the selected Rune bits. */
int probe(const unsigned char *data, unsigned int bytes, unsigned int limit,
    unsigned int id, unsigned int rune, unsigned int *answer)
{
    DisStringPoolView pool, before;
    DisRuneView view, saved;
    memset(&pool, 0x5a, sizeof(pool));
    memcpy(&before, &pool, sizeof(pool));
    if(!disstringpoolcheck(data, bytes, limit, &pool))
        return memcmp(&pool, &before, sizeof(pool)) == 0 ? 0 : -1;
    memset(&view, 0x6b, sizeof(view));
    memcpy(&saved, &view, sizeof(view));
    if(!disstringpoolget(&pool, id, &view))
        return memcmp(&view, &saved, sizeof(view)) == 0 ? 2 : -1;
    return disruneat(&view, rune, answer) ? 1 : 3;
}


/* Compare complete checked pools without a Unicode or host-signed conversion. */
int compare(const unsigned char *data, unsigned int bytes,
    unsigned int left, unsigned int right, int *order)
{
    DisStringPoolView pool;
    if(!disstringpoolcheck(data, bytes, 10000, &pool))
        return 0;
    return disstringpoolcompare(&pool, left, right, order);
}


/* Expose both shared writers so byte construction does not duplicate C logic. */
int emit(unsigned char *output, unsigned int capacity, const DisStringPoolRecord *records,
    unsigned int count, unsigned int *written)
{
    return disstringpoolwrite(output, capacity, records, count, written);
}
int rune(unsigned char *output, unsigned int capacity, const unsigned int *values,
    unsigned int count, unsigned int *written)
{
    return disrunewrite(output, capacity, values, count, written);
}
'''


# Native pointers only describe this host test adapter, never serialized fields.
class Record(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("bytes", ctypes.c_uint)]


# Encode exact independent BE records with arbitrary u32 bits, not Unicode text.
def record(values):
    return len(values).to_bytes(4, "big") + b"".join(v.to_bytes(4, "big") for v in values)


# Compile the real header and check bytes/results against mathematical oracles.
def main():
    keys = [0, 255, 0xd800, 0x10ffff, 0x110000, 0x7fffffff, 0x80000000, 0xffffffff]
    corpus = [[], [], keys] + [[v] for v in keys] + [[v, 0] for v in keys]
    corpus += [[(i * 0x9e3779b9) & 0xffffffff for i in range(n)] for n in range(12)]
    golden = json.loads((ROOT/"libinterp/tests/dis64/string-pool-golden.json").read_text())
    expected_golden = len(golden["strings"]).to_bytes(4, "big") + b"".join(record(v) for v in golden["strings"])
    if expected_golden != bytes.fromhex(golden["hex"]):
        raise RuntimeError("hand-written string-pool golden bytes disagree")
    signed_key = lambda values: tuple(v if v < 0x80000000 else v-0x100000000 for v in values)
    if sorted(range(len(golden["strings"])), key=lambda i: signed_key(golden["strings"][i])) != golden["signed_order_ids"]:
        raise RuntimeError("hand-written string-pool golden order disagrees")
    checks = 0
    with tempfile.TemporaryDirectory(prefix="dis-string-pool-") as directory:
        source, library = Path(directory)/"pool.c", Path(directory)/"pool.so"
        source.write_text(ADAPTER)
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        subprocess.run(command + ["-std=c99", "-shared", "-fPIC", "-I"+str(ROOT/"include"),
                                 str(source), "-o", str(library)], check=True)
        loaded = ctypes.CDLL(str(library))
        probe, compare, emit, rune = loaded.probe, loaded.compare, loaded.emit, loaded.rune
        probe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint,
                          ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        compare.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint,
                            ctypes.POINTER(ctypes.c_int)]
        emit.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint,
                         ctypes.POINTER(ctypes.c_uint)]
        rune.argtypes = emit.argtypes
        samples = [corpus[:size] for size in (0, 1, 2, 7, len(corpus))] + [golden["strings"]]
        for values in samples:
            size = len(values)
            buffers = [ctypes.create_string_buffer(record(v)) for v in values]
            entries = (Record*size)(*[Record(ctypes.cast(p, ctypes.c_void_p), len(record(v)))
                                      for p, v in zip(buffers, values)])
            expected = size.to_bytes(4, "big") + b"".join(record(v) for v in values)
            for capacity in range(len(expected)+2):
                output = ctypes.create_string_buffer(b"\xa5"*(len(expected)+8))
                written = ctypes.c_uint(0x12345678)
                valid = capacity >= len(expected)
                if emit(output, capacity, entries, size, ctypes.byref(written)) != int(valid):
                    raise RuntimeError("pool writer capacity differs")
                wanted = expected+b"\xa5"*8+b"\0" if valid else b"\xa5"*(len(expected)+8)+b"\0"
                if output.raw != wanted or written.value != (len(expected) if valid else 0x12345678):
                    raise RuntimeError("pool writer changed bytes on failure")
                checks += 1
            invalid = [expected[:end] for end in range(len(expected))] + [expected+b"\0"]
            for payload in [expected]+invalid:
                for limit in (0, max(0, size-1), size, 10000):
                    for index in range(size+1):
                        output = ctypes.c_uint(0x12345678)
                        valid = payload == expected and limit >= size
                        want = 0 if not valid else 2 if index == size else 1 if values[index] else 3
                        result = probe(payload, len(payload), limit, index, 0, ctypes.byref(output))
                        if result != want or output.value != (values[index][0] if want == 1 else 0x12345678):
                            raise RuntimeError("pool reader/resource/getter publication differs")
                        checks += 1
            for left, a in enumerate(values):
                signed_a = [v if v < 0x80000000 else v-0x100000000 for v in a]
                for right, b in enumerate(values):
                    signed_b = [v if v < 0x80000000 else v-0x100000000 for v in b]
                    order = ctypes.c_int(12345)
                    want = (signed_a > signed_b)-(signed_a < signed_b)
                    if compare(expected, len(expected), left, right, ctypes.byref(order)) != 1 or order.value != want:
                        raise RuntimeError("pool ID/content order differs")
                    checks += 1
                order = ctypes.c_int(12345)
                if compare(expected, len(expected), left, size, ctypes.byref(order)) or order.value != 12345:
                    raise RuntimeError("missing pool ID published order")
                checks += 1
            # Shifted spans cannot depend on host alignment; count corruption
            # and trailing record bytes must not be hidden by the pool count.
            shifted = ctypes.create_string_buffer(b"!"+expected)
            output = ctypes.c_uint(0x12345678)
            result = probe(ctypes.byref(shifted, 1), len(expected), size, size, 0, ctypes.byref(output))
            if result != 2 or output.value != 0x12345678:
                raise RuntimeError("unaligned pool index rejection differs")
            bad = b"\xff\xff\xff\xff"+expected[4:]
            if probe(bad, len(bad), 0xffffffff, 0, 0, ctypes.byref(output)) != 0:
                raise RuntimeError("oversized pool count admitted")
            checks += 2
        # Malformed serialized inputs reject before the pool writer touches output.
        for payload in (b"", b"\0\0\0", record([1])[:-1], record([])+b"\0"):
            storage = ctypes.create_string_buffer(payload)
            entry = Record(ctypes.cast(storage, ctypes.c_void_p), len(payload))
            output = ctypes.create_string_buffer(b"KEEP"*8)
            written = ctypes.c_uint(12345)
            if emit(output, 32, ctypes.byref(entry), 1, ctypes.byref(written)) or written.value != 12345 or output.raw != b"KEEP"*8+b"\0":
                raise RuntimeError("bad source record changed pool writer output")
            checks += 1
        # Direct codec composition preserves arbitrary Rune bits and empty records.
        for values in corpus:
            array = (ctypes.c_uint*len(values))(*values)
            storage = ctypes.create_string_buffer(len(record(values)))
            written = ctypes.c_uint(12345)
            if not rune(storage, len(storage), array, len(values), ctypes.byref(written)) or storage.raw != record(values):
                raise RuntimeError("shared Rune writer composition differs")
            checks += 1
    print("PASS Dis64 string pool:", checks, "bytes/limits/IDs/order/capacity/atomicity checks")


if __name__ == "__main__":
    main()
