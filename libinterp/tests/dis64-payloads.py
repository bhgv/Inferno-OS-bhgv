#!/usr/bin/env python3
"""Check agreed module/type/code framing and whole pilot linking without a VM."""
import ctypes
import json
import os
import re
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = r'''
#include <string.h>
#include "isa.h"
#include "dis64-pilot.h"


/* Check one metadata record and preserve all caller output on rejection. */
int
moduleprobe(const unsigned char *data, unsigned int bytes, unsigned int *fields)
{
    Dis64ModuleInfo info, saved;

    memset(&info, 0x5a, sizeof(info));
    memcpy(&saved, &info, sizeof(info));
    if(!dis64modulecheck(data, bytes, &info))
        return memcmp(&info, &saved, sizeof(info)) == 0 ? 0 : -1;
    fields[0]=info.name;
    fields[1]=info.profile;
    fields[2]=info.databytes;
    fields[3]=info.datatype;
    fields[4]=info.entry;
    fields[5]=info.frametype;
    return 1;
}


/* Expose the actual metadata writer with typed caller-owned input. */
int
modulewrite(unsigned char *out, unsigned int capacity,
    const Dis64ModuleInfo *info, unsigned int *written)
{
    return dis64modulewrite(out, capacity, info, written);
}


/* Check a complete descriptor table and a selected ordinal. */
int
typesprobe(const unsigned char *data, unsigned int bytes, unsigned int id,
    unsigned int limit, unsigned int *fields)
{
    Dis64TypeTable types, saved;
    Dis64Type type, before;

    memset(&types, 0x5a, sizeof(types));
    memcpy(&saved, &types, sizeof(types));
    if(!dis64typescheck(data, bytes, 64, limit, &types))
        return memcmp(&types, &saved, sizeof(types)) == 0 ? 0 : -1;
    memset(&type, 0x6b, sizeof(type));
    memcpy(&before, &type, sizeof(type));
    if(!dis64typeget(&types, id, &type))
        return memcmp(&type, &before, sizeof(type)) == 0 ? 2 : -1;
    fields[0]=types.count;
    fields[1]=type.bytes;
    fields[2]=type.align;
    fields[3] = type.mapbytes;
    return 1;
}


/* Exercise the production descriptor emitter's complete preflight. */
int
typeswrite(unsigned char *out, unsigned int capacity,
    const Dis64Type *types, unsigned int count, unsigned int *written)
{
    return dis64typeswrite(out, capacity, types, count, written);
}


/* Resolve one instruction after full role/framing validation. */
int
codeprobe(const unsigned char *data, unsigned int bytes,
    unsigned int mpbytes, unsigned int fpbytes, unsigned int pc, long *fields)
{
    Dis64CodeView code, saved;
    Dis64Instruction instruction, before;

    memset(&code, 0x5a, sizeof(code));
    memcpy(&saved, &code, sizeof(code));
    if(!dis64codecheck(data, bytes, 1024, mpbytes, fpbytes, &code))
        return memcmp(&code, &saved, sizeof(code)) == 0 ? 0 : -1;
    memset(&instruction, 0x6b, sizeof(instruction));
    memcpy(&before, &instruction, sizeof(instruction));
    if(!dis64codeget(&code, pc, &instruction))
        return memcmp(&instruction, &before, sizeof(instruction)) == 0 ? 2 : -1;
    fields[0]=code.count;
    fields[1]=instruction.op;
    fields[2]=instruction.mode;
    fields[3]=instruction.middle;
    fields[4]=instruction.source;
    fields[5]=instruction.destination;
    return 1;
}


/* Check signed30 reading independently of executable code framing. */
int
operandprobe(const unsigned char *data, unsigned int bytes, long *value, unsigned int *used)
{
    return dis64operandread(data, bytes, value, used);
}


/* Check all pilot references before publishing the immutable linked view. */
int
pilotprobe(const unsigned char *data, unsigned int bytes, unsigned int *fields)
{
    Dis64Pilot pilot, saved;

    memset(&pilot, 0x5a, sizeof(pilot));
    memcpy(&saved, &pilot, sizeof(pilot));
    if(!dis64pilotcheck(data, bytes, 4096, 1024, &pilot))
        return memcmp(&pilot, &saved, sizeof(pilot)) == 0 ? 0 : -1;
    fields[0]=pilot.module.databytes;
    fields[1]=pilot.frame.bytes;
    fields[2]=pilot.module.entry;
    fields[3]=pilot.code.count;
    return 1;
}


/* Produce the golden using shared metadata/type/string/envelope writers. */
int
buildpilot(unsigned char *out, unsigned int capacity, unsigned int *written)
{
    unsigned char metadata[24], code[]={0,0,0,2,IMOVW,SRC(AIMM)|DST(AMP),42,0,IEXIT,SRC(AXXX)|DST(AXXX)};
    unsigned char typesbytes[32], runes[24], strings[32], zero[4]={0}, map=0;
    unsigned int name[]={80,105,108,111,116}, nmodule, ntypes, nrunes, nstrings;
    Dis64ModuleInfo module={0,1,4,0,0,1};
    Dis64Type types[]={{4,4,&map,1},{64,8,&map,1}};
    DisStringPoolRecord record;
    Dis64Payload sections[5];
    if(!dis64modulewrite(metadata,sizeof(metadata),&module,&nmodule) ||
       !dis64typeswrite(typesbytes,sizeof(typesbytes),types,2,&ntypes) ||
       !disrunewrite(runes,sizeof(runes),name,5,&nrunes))
        return 0;
    record.data = runes;
    record.bytes = nrunes;
    if(!disstringpoolwrite(strings,sizeof(strings),&record,1,&nstrings))
        return 0;
    sections[0]=(Dis64Payload){Dis64Module,1,metadata,nmodule};
    sections[1]=(Dis64Payload){Dis64Code,1,code,sizeof(code)};
    sections[2]=(Dis64Payload){Dis64Types,1,typesbytes,ntypes};
    sections[3]=(Dis64Payload){Dis64Data,1,zero,4};
    sections[4]=(Dis64Payload){Dis64Strings,1,strings,nstrings};
    return dis64write(out,capacity,0,0,sections,5,written);
}
'''


# Host structure for metadata fields; target bytes are checked independently.
class Module(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
                ("name", "profile", "databytes", "datatype", "entry", "frametype")]


# Borrowed descriptor input includes a native pointer that is never serialized.
class Type(ctypes.Structure):
    _fields_ = [("bytes", ctypes.c_uint), ("align", ctypes.c_uint),
                ("map", ctypes.c_void_p), ("mapbytes", ctypes.c_uint)]


# Build signed operands with independent unsigned masking at each width boundary.
def operand(value):
    length = 1 if -64 <= value <= 63 else 2 if -8192 <= value <= 8191 else 4
    bits = value & ((1 << (7 if length == 1 else 14 if length == 2 else 30)) - 1)
    raw = bytearray(bits.to_bytes(length, "big"))
    raw[0] |= 0 if length == 1 else 0x80 if length == 2 else 0xc0
    return bytes(raw)


# Compile actual helpers; use explicit fixtures and mathematical range oracles.
def main():
    golden = bytes.fromhex((ROOT / "libinterp/tests/dis64/pilot42.hex").read_text())
    checks = 0
    with tempfile.TemporaryDirectory(prefix="dis64-payloads-") as temporary:
        source = Path(temporary) / "probe.c"
        library = Path(temporary) / "probe.so"
        source.write_text(SOURCE)
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        command += ["-std=c99", "-Wno-unused-function", "-shared", "-fPIC", "-I" + str(ROOT / "include"),
                    str(source), "-o", str(library)]
        subprocess.run(command, check=True)
        lib = ctypes.CDLL(str(library))
        lib.moduleprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        lib.modulewrite.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(Module), ctypes.POINTER(ctypes.c_uint)]
        lib.typesprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        lib.typeswrite.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(Type), ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        lib.codeprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_long)]
        lib.operandprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_uint)]
        lib.pilotprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        lib.buildpilot.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        for profile in (0, 1, 2):
            for size in (0, 4):
                module = Module(3, profile, size, 7, 11, 13)
                values = [3, profile, size] + ([7] if size else []) + ([11, 13] if profile == 1 else [])
                expected = b"".join(struct.pack(">I", value) for value in values)
                for capacity in range(27):
                    out = ctypes.create_string_buffer(b"\xa5" * 32)
                    written = ctypes.c_uint(12345)
                    valid = profile <= 1 and capacity >= len(expected)
                    ok = lib.modulewrite(out, capacity, ctypes.byref(module), ctypes.byref(written))
                    wanted = expected + b"\xa5" * (32-len(expected)) + b"\0" if valid else b"\xa5" * 32 + b"\0"
                    if ok != int(valid) or out.raw != wanted or written.value != (len(expected) if valid else 12345):
                        raise RuntimeError("module writer mismatch")
                    checks += 1
                for payload in [expected] + [expected[:end] for end in range(len(expected))] + [expected + b"\0"]:
                    output = (ctypes.c_uint * 6)(*([12345] * 6))
                    valid = profile <= 1 and payload == expected
                    if lib.moduleprobe(payload, len(payload), output) != int(valid):
                        raise RuntimeError("module framing mismatch")
                    expected_fields = [3, profile, size, 7 if size else 0,
                                       11 if profile == 1 else 0, 13 if profile == 1 else 0]
                    if list(output) != (expected_fields if valid else [12345] * 6):
                        raise RuntimeError("module fields/publication mismatch")
                    checks += 1
        for size in range(129):
            for alignment in (0, 1, 2, 3, 4, 8, 16):
                mapbytes = (size + 63) // 64
                bitmap = ctypes.create_string_buffer(bytes(mapbytes))
                t = Type(size, alignment, ctypes.addressof(bitmap), mapbytes)
                expected = struct.pack(">III", 1, size, alignment) + bytes(mapbytes)
                valid = alignment in (1, 2, 4, 8) and size % alignment == 0 and (size != 0 or alignment == 1)
                for capacity in (0, len(expected)-1, len(expected), len(expected)+1):
                    out = ctypes.create_string_buffer(b"\xa5" * (len(expected)+4))
                    written = ctypes.c_uint(12345)
                    accepted = valid and capacity >= len(expected)
                    if lib.typeswrite(out, capacity, ctypes.byref(t), 1, ctypes.byref(written)) != int(accepted):
                        raise RuntimeError("type writer classification mismatch")
                    wanted = (expected + b"\xa5" * 4 + b"\0" if accepted else b"\xa5" * (len(expected)+4) + b"\0")
                    if out.raw != wanted or written.value != (len(expected) if accepted else 12345):
                        raise RuntimeError("type writer byte/publication mismatch")
                    checks += 1
                output = (ctypes.c_uint * 4)(*([12345] * 4))
                if lib.typesprobe(expected, len(expected), 0, 4096, output) != int(valid):
                    raise RuntimeError("type reader alignment mismatch")
                if valid and list(output) != [1, size, alignment, mapbytes]:
                    raise RuntimeError("type getter mismatch")
                checks += 1
        values = [-536870912, -8193, -8192, -65, -64, -1, 0, 63, 64, 8191, 8192, 536870911]
        values += [((index * 2654435761) & 0x3fffffff) - 0x20000000 for index in range(2048)]
        for value in values:
            image = operand(value)
            for length in range(len(image)+1):
                decoded, used = ctypes.c_long(12345), ctypes.c_uint(12345)
                ok = lib.operandprobe(image, length, ctypes.byref(decoded), ctypes.byref(used))
                valid = length == len(image)
                if ok != int(valid) or decoded.value != (value if valid else 12345) or used.value != (len(image) if valid else 12345):
                    raise RuntimeError("signed operand decoding mismatch")
                checks += 1
        # MOVW immediate to each aligned/unaligned/out-of-bounds MP destination.
        for value in (-536870912, -64, 0, 42, 63, 8192, 536870911):
            for destination in range(18):
                code = struct.pack(">I", 2) + bytes((45, 16)) + operand(value) + operand(destination) + bytes((15, 27))
                output = (ctypes.c_long * 6)(*([12345] * 6))
                valid = destination % 4 == 0 and destination + 4 <= 16
                ok = lib.codeprobe(code, len(code), 16, 64, 0, output)
                if ok != int(valid) or (valid and list(output) != [2,45,16,0,value,destination]):
                    raise RuntimeError("code role/bounds mismatch")
                checks += 1
        names = re.findall(r"^\s*(I[A-Z0-9]+)\s*,", (ROOT / "include/isa.h").read_text().split("MAXDIS")[0], re.M)
        opcodes = {name: index for index, name in enumerate(names)}
        if opcodes["IMOVW"] != 45 or opcodes["IEXIT"] != 15:
            raise RuntimeError("golden ISA numbers changed")
        for name in ("IMOVW", "IMOVL", "IMOVF", "IADDW", "IADDL", "ISHLL", "IBEQW", "IBEQF", "IJMP", "IEXIT", "INOP", "ICALL"):
            move = name.startswith("IMOV")
            branch = name.startswith("IBE")
            arithmetic = name in ("IADDW", "IADDL", "ISHLL")
            floating = name in ("IMOVF", "IBEQF")
            for sm in range(8):
                for dm in range(8):
                    for mm in (0, 0x40, 0x80, 0xc0):
                        mode = mm | (sm << 3) | dm
                        sv = 64 if sm == 1 else 1 if sm == 2 else 0
                        dv = 64 if dm == 1 else 1 if dm == 2 else 0
                        mv = 64 if mm == 0x80 else 1 if mm == 0x40 else 0
                        image = bytes((opcodes[name], mode))
                        if mm:
                            image += operand(mv)
                        if sm != 3:
                            image += operand(sv)
                        if dm != 3:
                            image += operand(dv)
                        code = struct.pack(">I", 2) + image + bytes((15, 27))
                        if move:
                            valid = sm in (0, 1, 2) and (not floating or sm != 2) and dm in (0, 1) and mm == 0
                        elif arithmetic:
                            valid = sm in (0, 1, 2) and dm in (0, 1)
                        elif branch:
                            valid = sm in (0, 1, 2) and (not floating or sm != 2) and dm == 2 and mm != 0 and (not floating or mm != 0x40)
                        elif name == "IJMP":
                            valid = sm == 3 and dm == 2 and mm == 0
                        elif name == "IEXIT":
                            valid = sm == 3 and dm == 3 and mm == 0
                        else:
                            valid = False
                        output = (ctypes.c_long * 6)(*([12345] * 6))
                        ok = lib.codeprobe(code, len(code), 16, 96, 0, output)
                        if ok != int(valid):
                            raise RuntimeError("operand role matrix mismatch: " + str((name, sm, dm, mm)))
                        if not valid and list(output) != [12345] * 6:
                            raise RuntimeError("role rejection published output")
                        checks += 1
        for capacity in range(len(golden)+2):
            out = ctypes.create_string_buffer(b"\xa5" * (len(golden)+4))
            written = ctypes.c_uint(12345)
            valid = capacity >= len(golden)
            if lib.buildpilot(out, capacity, ctypes.byref(written)) != int(valid):
                raise RuntimeError("pilot writer capacity mismatch")
            wanted = golden + b"\xa5" * 4 + b"\0" if valid else b"\xa5" * (len(golden)+4) + b"\0"
            if out.raw != wanted:
                raise RuntimeError("produced pilot differs from hand-written golden")
            checks += 1
        bad = [golden[:end] for end in range(len(golden))] + [golden + b"\0"]
        for offset, width, value in ((92,4,1),(96,4,2),(100,4,8),(104,4,2),(108,4,2),(112,4,0),
                                     (134,4,3),(138,1,128),(139,4,56),(147,1,1),
                                     (120,1,0),(121,1,18),(123,1,1),(123,1,4),(148,4,1),(152,4,0)):
            image = bytearray(golden)
            image[offset:offset+width] = value.to_bytes(width, "big")
            bad.append(bytes(image))
        for payload, valid in [(golden, True)] + [(image, False) for image in bad]:
            output = (ctypes.c_uint * 4)(*([12345] * 4))
            ok = lib.pilotprobe(payload, len(payload), output)
            if ok != int(valid) or list(output) != ([4,64,0,2] if valid else [12345] * 4):
                raise RuntimeError("whole pilot semantic/publication mismatch")
            checks += 1
    print("PASS Dis64 payloads:", checks, "module/type/operand/code/pilot cases; no VM execution claimed")


if __name__ == "__main__":
    main()
