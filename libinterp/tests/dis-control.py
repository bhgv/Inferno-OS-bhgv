#!/usr/bin/env python3
"""Check production candidate table helpers against independent golden payloads."""
import ctypes
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
KINDS = {"i32": 0, "big": 1, "string": 2, "goto": 3}
SOURCE = r'''
#include <string.h>
#include "dis-control.h"


/* Check the helper's publication guarantee as well as acceptance and row count. */
int probe(int kind, const unsigned char *data, unsigned int bytes,
    unsigned int instructions, DisControlCompare compare, unsigned int *count)
{
    DisControlView view, saved;
    int ok;

    memset(&view, 0x5a, sizeof(view));
    memcpy(&saved, &view, sizeof(view));
    ok = discontrolcheck(kind, data, bytes, instructions, compare, 0, &view);
    if(!ok){
        if(memcmp(&view, &saved, sizeof(view)) != 0)
            return -1;
        return 0;
    }
    *count = view.count;
    return 1;
}
'''


# Decode the candidate rows independently using Python signed integers and strings.
def oracle(kind, payload, instructions, strings):
    width = 8 if kind == "big" else 4
    rowbytes = 4 if kind == "goto" else width * 2 + 4
    trailer = 0 if kind == "goto" else 4
    if len(payload) < 4 + trailer or instructions == 0:
        return False
    count = int.from_bytes(payload[:4], "big")
    if 4 + count * rowbytes + trailer != len(payload):
        return False
    if trailer and int.from_bytes(payload[-4:], "big") >= instructions:
        return False
    previous = None
    for index in range(count):
        row = payload[4 + index * rowbytes:4 + (index + 1) * rowbytes]
        if int.from_bytes(row[-4:], "big") >= instructions:
            return False
        if kind == "goto":
            continue
        if kind == "string":
            lo, hi = (int.from_bytes(row[at:at + 4], "big") for at in (0, 4))
            if max(lo, hi) >= len(strings):
                return False
            lo, hi = strings[lo], strings[hi]
        else:
            lo = int.from_bytes(row[:width], "big", signed=True)
            hi = int.from_bytes(row[width:width * 2], "big", signed=True)
        if lo > hi or (previous is not None and previous >= lo):
            return False
        previous = hi
    return True


# Compile the actual helper and compare complete, truncated and mutated payloads.
def main():
    fixture = json.loads((ROOT / "libinterp/tests/dis64/control-golden.json").read_text())
    strings = fixture["strings_by_id"]
    callback_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p,
                                    ctypes.c_uint, ctypes.c_uint,
                                    ctypes.POINTER(ctypes.c_int))

    # Resolve IDs independently; the table helper never assumes numeric ID order.
    def compare(context, left, right, order):
        if left >= len(strings) or right >= len(strings):
            return 0
        order[0] = (strings[left] > strings[right]) - (strings[left] < strings[right])
        return 1

    callback = callback_type(compare)
    cases = 0
    with tempfile.TemporaryDirectory(prefix="dis-control-") as temporary:
        source = Path(temporary) / "probe.c"
        library = Path(temporary) / "probe.so"
        source.write_text(SOURCE)
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        command += ["-shared", "-fPIC", "-I" + str(ROOT / "include"), str(source), "-o", str(library)]
        subprocess.run(command, check=True)
        probe = ctypes.CDLL(str(library)).probe
        probe.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                          callback_type, ctypes.POINTER(ctypes.c_uint)]
        probe.restype = ctypes.c_int
        for validity in ("valid", "invalid"):
            for entry in fixture[validity]:
                original = bytes.fromhex(entry["hex"])
                expected = oracle(entry["kind"], original, 4, strings)
                if expected != (validity == "valid"):
                    raise RuntimeError("incorrect hand-written golden: " + entry["name"])
                variants = [original] + [original[:end] for end in range(len(original))]
                variants += [original + b"\x00"]
                for at in range(len(original)):
                    for value in (0, 0x7f, 0x80, 0xff):
                        changed = bytearray(original)
                        changed[at] = value
                        variants.append(bytes(changed))
                for payload in variants:
                    for instructions in (0, 1, 4, 0xffffffff):
                        want = oracle(entry["kind"], payload, instructions, strings)
                        output = ctypes.c_uint(0x12345678)
                        storage = ctypes.create_string_buffer(payload)
                        result = probe(KINDS[entry["kind"]], storage, len(payload),
                                       instructions, callback, ctypes.byref(output))
                        if result != int(want):
                            raise RuntimeError("acceptance/publication mismatch: " + entry["name"])
                        if not want and output.value != 0x12345678:
                            raise RuntimeError("failed validation changed caller output")
                        if want and output.value != int.from_bytes(payload[:4], "big"):
                            raise RuntimeError("wrong row count")
                        cases += 1
    print("PASS Dis64 candidate typed tables:", cases,
          "golden/truncation/mutation/PC cases; active loader unchanged")


if __name__ == "__main__":
    main()
