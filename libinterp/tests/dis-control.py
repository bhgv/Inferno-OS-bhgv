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


/* Exercise selection only after the common validation contract has succeeded. */
int query(int kind, const unsigned char *data, unsigned int bytes,
    unsigned int instructions, DisControlCompare compare,
    const unsigned char *key, unsigned int index, unsigned int *target)
{
    DisControlView view;

    if(!discontrolcheck(kind, data, bytes, instructions, compare, 0, &view))
        return 0;
    if(kind == DisControlGoto)
        return discontrolgoto(&view, index, target);
    return discontrolcase(kind, &view, key, compare, 0, target);
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
        loaded = ctypes.CDLL(str(library))
        probe = loaded.probe
        probe.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                          callback_type, ctypes.POINTER(ctypes.c_uint)]
        probe.restype = ctypes.c_int
        query = loaded.query
        query.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                          callback_type, ctypes.c_void_p, ctypes.c_uint,
                          ctypes.POINTER(ctypes.c_uint)]
        query.restype = ctypes.c_int
        selections = 0
        selection_entries = list(fixture["valid"])
        for kind, width in (("i32", 4), ("big", 8)):
            for count in range(65):
                image = count.to_bytes(4, "big")
                for index in range(count):
                    lo = (index * 4 - count * 2).to_bytes(width, "big", signed=True)
                    hi = (index * 4 - count * 2 + 1).to_bytes(width, "big", signed=True)
                    image += lo + hi + (index % 3).to_bytes(4, "big")
                image += (3).to_bytes(4, "big")
                selection_entries.append({"name": kind + "-search-" + str(count),
                                          "kind": kind, "hex": image.hex(), "queries": count})
        for entry in selection_entries:
            payload = bytes.fromhex(entry["hex"])
            kind = entry["kind"]
            width = 8 if kind == "big" else 4
            rowbytes = 4 if kind == "goto" else width * 2 + 4
            count = int.from_bytes(payload[:4], "big")
            values = range(len(strings) + 1) if kind == "string" else (
                -2 ** (width * 8 - 1), -100, -1, 0, 1, 100, 2 ** (width * 8 - 1) - 1)
            if kind == "goto":
                values = range(count + 2)
            if "queries" in entry:
                values = range(-count * 2 - 2, count * 2 + 3)
            for value in values:
                want_ok = not (kind == "string" and value >= len(strings))
                if kind == "goto":
                    want_ok = value < count
                    wanted = int.from_bytes(payload[4 + value * 4:8 + value * 4], "big")
                else:
                    wanted = int.from_bytes(payload[-4:], "big")
                    for row in range(count):
                        image = payload[4 + row * rowbytes:4 + (row + 1) * rowbytes]
                        if kind == "string":
                            lo, hi = (strings[int.from_bytes(image[at:at + 4], "big")]
                                      for at in (0, 4))
                            selected = want_ok and lo <= strings[value] <= hi
                        else:
                            lo = int.from_bytes(image[:width], "big", signed=True)
                            hi = int.from_bytes(image[width:width * 2], "big", signed=True)
                            selected = lo <= value <= hi
                        if selected:
                            wanted = int.from_bytes(image[-4:], "big")
                            break
                key = (value % (1 << (width * 8))).to_bytes(width, "big")
                output = ctypes.c_uint(0x12345678)
                data_storage = ctypes.create_string_buffer(payload)
                key_storage = ctypes.create_string_buffer(key)
                ok = query(KINDS[kind], data_storage, len(payload), 4, callback,
                           key_storage, value if kind == "goto" else 0, ctypes.byref(output))
                if ok != int(want_ok) or output.value != (wanted if want_ok else 0x12345678):
                    raise RuntimeError("selection mismatch: " + entry["name"])
                selections += 1

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
          "golden/truncation/mutation/PC cases;", selections,
          "case/goto selections; active loader unchanged")


if __name__ == "__main__":
    main()
