#!/usr/bin/env python3
"""Check the agreed envelope codec against independent bytes and structural rules."""
import ctypes
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MAGIC = bytes.fromhex("44697336340d0a1a")
SOURCE = r'''
#include <string.h>
#include "dis64.h"


/* Explicit fixture feature registry; mode two accepts every structurally valid name. */
static int
supports(void *context, const Dis64Extension *extension)
{
    int mode = *(int*)context;

    if(mode == 2)
        return 1;
    if(mode != 1)
        return 0;
    if(extension->namebytes == 13 && memcmp(extension->name, "inferno.alpha", 13) == 0)
        return extension->major == 2 && extension->minor <= 3;
    if(extension->namebytes == 12 && memcmp(extension->name, "flashvm.beta", 12) == 0)
        return extension->major == 1 && extension->minor == 0;
    return 0;
}


/* Verify unchanged views and section outputs on both kinds of failed publication. */
int
probe(const unsigned char *data, unsigned int bytes, unsigned int maxextensions,
    unsigned int maxsections, int mode, unsigned int index, unsigned int *info)
{
    Dis64View view, saved;
    Dis64Section section, before;

    memset(&view, 0x5a, sizeof(view));
    memcpy(&saved, &view, sizeof(view));
    if(!dis64check(data, bytes, maxextensions, maxsections, supports, &mode, &view))
        return memcmp(&view, &saved, sizeof(view)) == 0 ? 0 : -1;
    memset(&section, 0x6b, sizeof(section));
    memcpy(&before, &section, sizeof(section));
    if(!dis64section(&view, index, &section))
        return memcmp(&section, &before, sizeof(section)) == 0 ? 2 : -1;
    info[0] = view.headerbytes;
    info[1] = view.extensions;
    info[2] = view.sections;
    info[3] = section.kind;
    info[4] = section.flags;
    info[5] = section.offset;
    info[6] = section.bytes;
    return 1;
}


/* Exercise the production writer directly, without a parallel C serializer. */
int
emit(unsigned char *output, unsigned int capacity, const Dis64Extension *extensions,
    unsigned int ne, const Dis64Payload *sections, unsigned int ns, unsigned int *written)
{
    return dis64write(output, capacity, extensions, ne, sections, ns, written);
}

/* Expose compiled IDs so the normative JSON and C namespace cannot drift. */
void
registry(unsigned char *magic, unsigned int *values)
{
    unsigned int i;
    const unsigned int accepted[] = {Dis64Major, Dis64Minor, Dis64Abi,
        Dis64HeaderBytes, Dis64DirectoryBytes, Dis64Required,
        Dis64Module, Dis64Code, Dis64Types, Dis64Data, Dis64Imports,
        Dis64Exports, Dis64Strings, Dis64Control, Dis64Debug, Dis64Provenance};

    for(i=0; i<8; i++)
        magic[i] = dis64magic[i];
    for(i=0; i<sizeof(accepted)/sizeof(accepted[0]); i++)
        values[i] = accepted[i];
}

'''


# Match the compiler host's pointer alignment while describing target values explicitly.
class Extension(ctypes.Structure):
    _fields_ = [("name", ctypes.c_void_p), ("namebytes", ctypes.c_uint),
                ("major", ctypes.c_uint), ("minor", ctypes.c_uint)]


# Payload spans are borrowed inputs; the emitted file contains no native pointer.
class Payload(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_uint), ("flags", ctypes.c_uint),
                ("data", ctypes.c_void_p), ("bytes", ctypes.c_uint)]


# Construct independent canonical bytes using only Python's explicit BE formats.
def image(sections, extensions=()):
    manifest = b"".join(struct.pack(">HHHH", len(name), major, minor, 0) + name
                        for name, major, minor in extensions)
    headerbytes = 32 + len(manifest) + 12 * len(sections)
    header = MAGIC + struct.pack(">HHIIIII", 1, 0, headerbytes, 1, 0,
                                 len(extensions), len(sections))
    directory = b""
    cursor = headerbytes
    for kind, flags, payload in sections:
        directory += struct.pack(">HHII", kind, flags, cursor, len(payload))
        cursor += len(payload)
    return header + manifest + directory + b"".join(payload for _, _, payload in sections)


# Independently validate byte slices and Python integers before any C invocation.
def oracle(data, mode=2, maxextensions=64, maxsections=64):
    if len(data) < 32 or data[:8] != MAGIC:
        return False
    major, minor, header, abi, flags, ne, ns = struct.unpack(">HHIIIII", data[8:32])
    if (major, minor, abi, flags) != (1, 0, 1, 0) or not 32 <= header <= len(data):
        return False
    if ne > maxextensions or ns > maxsections or ne > (header - 32) // 9:
        return False
    at = 32
    previous = b""
    features = {b"inferno.alpha": (2, 3), b"flashvm.beta": (1, 0)}
    for _ in range(ne):
        if at + 8 > header:
            return False
        size, major, minor, reserved = struct.unpack(">HHHH", data[at:at + 8])
        name = data[at + 8:at + 8 + size]
        at += 8 + size
        if (at > header or reserved or not 1 <= size <= 255 or len(name) != size or
                re.fullmatch(rb"[a-z][a-z0-9._-]*", name) is None or name <= previous):
            return False
        if mode == 0 or (mode == 1 and (name not in features or
                major != features[name][0] or minor > features[name][1])):
            return False
        previous = name
    if header - at != ns * 12:
        return False
    seen = set()
    cursor = header
    for _ in range(ns):
        kind, flags, offset, size = struct.unpack(">HHII", data[at:at + 12])
        at += 12
        if offset != cursor or cursor + size > len(data) or flags & ~1:
            return False
        known = kind in range(1, 9) or kind in (32768, 32769)
        if known and kind in seen:
            return False
        if kind in range(1, 9):
            if flags != 1 or (kind in (1, 2, 3) and size == 0) or (kind >= 5 and size < 4):
                return False
        elif flags != 0:
            return False
        seen.add(kind)
        cursor += size
    return cursor == len(data) and {1, 2, 3}.issubset(seen)


# Check exact bytes, immutable rejection, capacities, feature policy and tool output.
def main():
    golden = json.loads((ROOT / "libinterp/tests/dis64/envelope-golden.json").read_text())
    minimum = [(entry["kind"], entry["flags"], bytes.fromhex(entry["hex"]))
               for entry in golden["minimum_sections"]]
    if image(minimum) != bytes.fromhex(golden["minimum_hex"]):
        raise RuntimeError("inconsistent hand-written envelope golden")
    fixtures = [(minimum, ()), (minimum + [(4, 1, b"")], ()),
                (minimum + [(7, 1, bytes(4)), (60000, 0, b"")], ()),
                (minimum + [(32768, 0, b"opaque"), (32769, 0, b"")], ()),
                (minimum, ((b"flashvm.beta", 1, 0), (b"inferno.alpha", 2, 3))),
                (minimum, ((b"inferno.alpha", 2, 4),)),
                (minimum, ((b"other.feature", 1, 0),)),
                (minimum, ((b"inferno.alpha", 2, 0), (b"inferno.alpha", 2, 0))),
                (minimum, ((b"Wrong.Name", 1, 0),)),
                (minimum + [(2, 1, b"duplicate")], ()),
                (minimum + [(60000, 1, b"unknown-required")], ()),
                (minimum + [(5, 1, b"")], ()), (minimum[1:], ()),
                ([(1, 0, b"M"), (2, 1, b"C"), (3, 1, b"T")], ())]
    readerchecks = writerchecks = 0
    with tempfile.TemporaryDirectory(prefix="dis-envelope-") as temporary:
        directory = Path(temporary)
        source = directory / "probe.c"
        library = directory / "probe.so"
        source.write_text(SOURCE)
        command = shlex.split(os.environ.get("CC", "cc"))
        flags = shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        subprocess.run(command + flags + ["-shared", "-fPIC", "-I" + str(ROOT / "include"),
                                         str(source), "-o", str(library)], check=True)
        loaded = ctypes.CDLL(str(library))
        accepted = json.loads((ROOT / "doc/dis64-registry.json").read_text())
        compiled_magic = (ctypes.c_ubyte * 8)()
        compiled_ids = (ctypes.c_uint * 16)()
        registry = loaded.registry
        registry.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        registry(compiled_magic, compiled_ids)
        names = ("module", "code", "types", "data", "imports", "exports", "strings",
                 "control_tables", "debug", "provenance")
        expected_ids = [accepted[key] for key in ("format_major", "format_minor", "abi_id",
                                                "header_bytes", "directory_record_bytes",
                                                "section_required_flag")]
        expected_ids += [accepted["section_kinds"][name] for name in names]
        if bytes(compiled_magic) != bytes.fromhex(accepted["magic_hex"]) or list(compiled_ids) != expected_ids:
            raise RuntimeError("compiled Dis64 registry differs from accepted JSON")
        probe = loaded.probe
        probe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint,
                          ctypes.c_int, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        emit = loaded.emit
        emit.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint,
                         ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        for sections, extensions in fixtures:
            expected = image(sections, extensions)
            names = [ctypes.create_string_buffer(name) for name, _, _ in extensions]
            payloads = [ctypes.create_string_buffer(payload) for _, _, payload in sections]
            exts = (Extension * len(extensions))(*(Extension(ctypes.addressof(names[i]), len(name), major, minor)
                                                  for i, (name, major, minor) in enumerate(extensions)))
            inputs = (Payload * len(sections))(*(Payload(kind, flags, ctypes.addressof(payloads[i]), len(payload))
                                                 for i, (kind, flags, payload) in enumerate(sections)))
            for capacity in range(len(expected) + 2):
                output = ctypes.create_string_buffer(b"\xa5" * (len(expected) + 8))
                written = ctypes.c_uint(0x12345678)
                valid = oracle(expected) and capacity >= len(expected)
                ok = emit(output, capacity, exts, len(extensions), inputs, len(sections), ctypes.byref(written))
                wanted = (expected + b"\xa5" * 8 + b"\x00" if valid else
                          b"\xa5" * (len(expected) + 8) + b"\x00")
                if ok != int(valid) or output.raw != wanted or written.value != (
                        len(expected) if valid else 0x12345678):
                    raise RuntimeError("envelope writer byte/capacity/publication mismatch")
                writerchecks += 1
            variants = [expected] + [expected[:end] for end in range(len(expected))] + [expected + b"\x00"]
            for at in range(len(expected)):
                for value in (0, 1, 0x7f, 0x80, 0xff):
                    changed = bytearray(expected)
                    changed[at] = value
                    variants.append(bytes(changed))
            for payload in variants:
                for mode in (0, 1, 2):
                    valid = oracle(payload, mode)
                    for shift in (0, 1):
                        storage = ctypes.create_string_buffer(bytes(shift) + payload)
                        info = (ctypes.c_uint * 7)(*([0x12345678] * 7))
                        ok = probe(ctypes.addressof(storage) + shift, len(payload), 64, 64, mode, 0, info)
                        if ok != int(valid) or (not valid and list(info) != [0x12345678] * 7):
                            raise RuntimeError("envelope reader/oracle/publication mismatch")
                        if valid:
                            _, _, headerbytes, _, _, ne, ns = struct.unpack(">HHIIIII", payload[8:32])
                            kind, entryflags, offset, size = struct.unpack(">HHII", payload[headerbytes - ns * 12:headerbytes - ns * 12 + 12])
                            if list(info) != [headerbytes, ne, ns, kind, entryflags, offset, size]:
                                raise RuntimeError("reader section getter differs from directory bytes")
                        readerchecks += 1
        baseline = ctypes.create_string_buffer(image(minimum))
        info = (ctypes.c_uint * 7)(*([0x12345678] * 7))
        if probe(baseline, len(baseline.raw) - 1, 64, 64, 0, 3, info) != 2 or list(info) != [0x12345678] * 7:
            raise RuntimeError("out-of-range section publication")
        if probe(baseline, len(baseline.raw) - 1, 0, 2, 0, 0, info) != 0:
            raise RuntimeError("section resource limit ignored")
        feature = ctypes.create_string_buffer(image(minimum, ((b"inferno.alpha", 2, 0),)))
        if probe(feature, len(feature.raw) - 1, 0, 64, 2, 0, info) != 0:
            raise RuntimeError("extension resource limit ignored")
        tool = directory / "dis64inspect"
        subprocess.run(command + flags + ["-std=c99", "-I" + str(ROOT / "include"),
                       str(ROOT / "utils/dis64inspect/dis64inspect.c"), "-o", str(tool)], check=True)
        filename = directory / "envelope.dis64"
        rewritten = directory / "rewritten.dis64"
        filename.write_bytes(image(minimum))
        result = subprocess.run([str(tool), str(filename), "--rewrite", str(rewritten)],
                                check=True, capture_output=True, text=True)
        report = json.loads(result.stdout)
        if report["execution_supported"] or not report["envelope_only"] or rewritten.read_bytes() != filename.read_bytes():
            raise RuntimeError("inspector claimed execution or changed canonical bytes")
        filename.write_bytes(image(minimum, ((b"other.feature", 1, 0),)))
        if subprocess.run([str(tool), str(filename)], capture_output=True).returncode == 0:
            raise RuntimeError("inspector accepted unsupported mandatory extension")
    print("PASS Dis64 envelope:", readerchecks, "reader/oracle cases,", writerchecks,
          "writer capacity cases; exact golden and inspector rewrite; payloads not executed")


if __name__ == "__main__":
    main()
