#!/usr/bin/env python3
"""Check native initializer publication and exact scalar interface framing."""
import ctypes
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = r'''
#include <string.h>
#include "isa.h"
#include "dis64-pilot.h"
#include "dis64-bindings.h"


/* Validate the real initializer reader and optionally publish native object bytes. */
int
dataprobe(const unsigned char *data, unsigned int bytes, const Dis64Type *type,
    unsigned char *output, unsigned int capacity, const unsigned char *nilbytes)
{
    Dis64Initializers view, saved;
    memset(&view, 0x5a, sizeof(view));
    memcpy(&saved, &view, sizeof(view));
    if(!dis64initializerscheck(data, bytes, type, 128, &view))
        return memcmp(&view, &saved, sizeof(view)) == 0 ? 0 : -1;
    return dis64initializersapply(&view, type, output, capacity, nilbytes) ? 1 : 2;
}


/* Exercise writer preflight without duplicating its byte encoding. */
int
datawrite(unsigned char *output, unsigned int capacity, const Dis64Type *type,
    const Dis64Initializer *records, unsigned int count, unsigned int *written)
{
    return dis64initializerswrite(output, capacity, type, records, count, written);
}


/* Preserve caller fields on any signature-version/kind/extent failure. */
int
signatureprobe(const unsigned char *data, unsigned int bytes, unsigned int *fields)
{
    Dis64Signature signature, saved;
    memset(&signature, 0x5a, sizeof(signature));
    memcpy(&saved, &signature, sizeof(signature));
    if(!dis64signaturecheck(data, bytes, 64, &signature))
        return memcmp(&signature, &saved, sizeof(signature)) == 0 ? 0 : -1;
    fields[0] = signature.result;
    fields[1] = signature.parameters;
    return 1;
}


/* Use the shared versioned signature writer directly. */
int
signaturewrite(unsigned char *output, unsigned int capacity, unsigned int result,
    const unsigned int *parameters, unsigned int count, unsigned int *written)
{
    return dis64signaturewrite(output, capacity, result, parameters, count, written);
}


/* Compare complete records only after both real readers have accepted them. */
int
signatureequal(const unsigned char *a, unsigned int na, const unsigned char *b,
    unsigned int nb, int *equal)
{
    Dis64Signature left, right;
    if(!dis64signaturecheck(a, na, 64, &left) || !dis64signaturecheck(b, nb, 64, &right))
        return 0;
    return dis64signatureequal(&left, &right, equal);
}


/* Decode real pool/type context before testing bindings or its writer. */
int
bindingsprobe(const unsigned char *data, unsigned int bytes, unsigned int exports,
    const unsigned char *poolbytes, unsigned int npool, const unsigned char *typebytes,
    unsigned int ntypes, unsigned int index, unsigned int *fields)
{
    DisStringPoolView pool;
    Dis64TypeTable types;
    Dis64Bindings view, saved;
    Dis64Binding binding, before;
    if(!disstringpoolcheck(poolbytes, npool, 64, &pool) ||
       !dis64typescheck(typebytes, ntypes, 64, 4096, &types))
        return -2;
    memset(&view, 0x5a, sizeof(view));
    memcpy(&saved, &view, sizeof(view));
    if(!dis64bindingscheck(data, bytes, exports, &pool, &types, 2, 64, 64, &view))
        return memcmp(&view, &saved, sizeof(view)) == 0 ? 0 : -1;
    memset(&binding, 0x6b, sizeof(binding));
    memcpy(&before, &binding, sizeof(binding));
    if(!dis64bindingget(&view, index, &binding))
        return memcmp(&binding, &before, sizeof(binding)) == 0 ? 2 : -1;
    fields[0] = binding.module;
    fields[1] = binding.symbol;
    fields[2] = binding.entry;
    fields[3] = binding.frame;
    fields[4] = binding.signature.result;
    fields[5] = binding.signature.parameters;
    return 1;
}


/* Validate context before using the actual canonical binding writer. */
int
bindingswrite(unsigned char *output, unsigned int capacity, unsigned int exports,
    const Dis64Binding *records, unsigned int count, const unsigned char *poolbytes,
    unsigned int npool, const unsigned char *typebytes, unsigned int ntypes,
    unsigned int *written)
{
    DisStringPoolView pool;
    Dis64TypeTable types;
    if(!disstringpoolcheck(poolbytes, npool, 64, &pool) ||
       !dis64typescheck(typebytes, ntypes, 64, 4096, &types))
        return -2;
    return dis64bindingswrite(output, capacity, exports, records, count,
        &pool, &types, 2, 64, written);
}
'''


# These host structures carry borrowed spans, not serialized pointer fields.
class Type(ctypes.Structure):
    _fields_ = [("bytes", ctypes.c_uint), ("align", ctypes.c_uint),
                ("map", ctypes.c_void_p), ("mapbytes", ctypes.c_uint)]


# High bits are used only by eight-byte payload kinds.
class Initializer(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in ("offset", "kind", "high", "low")]


# Signature cached fields are independent from its canonical borrowed record bytes.
class Signature(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("bytes", ctypes.c_uint),
                ("result", ctypes.c_uint), ("parameters", ctypes.c_uint)]


# Absent import/export fields are not emitted as wire sentinels.
class Binding(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in ("module", "symbol", "entry", "frame")] + [("signature", Signature)]


# Serialize independent expected signatures with explicit BE fields.
def signature(result, parameters, version=1):
    return struct.pack(">III", version, result, len(parameters)) + b"".join(struct.pack(">I", kind) for kind in parameters)


# Construct the accepted non-interning string pool directly from Rune32 values.
def pool_image(names):
    return struct.pack(">I", len(names)) + b"".join(
        struct.pack(">I", len(name)) + b"".join(struct.pack(">I", ord(c)) for c in name) for name in names)


# Model geometry and aliasing mathematically, independently of C bitmap access.
def init_valid(size, alignment, bitmap, records):
    if alignment not in (1, 2, 4, 8) or size % alignment or (size == 0 and alignment != 1):
        return False
    references = [slot * 8 for slot in range(len(bitmap) * 8) if bitmap[slot // 8] & (128 >> (slot % 8))]
    if any(alignment < 8 or offset + 8 > size for offset in references):
        return False
    intervals = []
    for offset, kind, bits in records:
        width = {1: 4, 2: 8, 3: 8, 4: 4}.get(kind, 0)
        if not width or offset % width or offset + width > size or (kind == 4 and bits > 1):
            return False
        if any(offset < other + 8 and other < offset + width for other in references):
            return False
        if any(offset < end and start < offset + width for start, end in intervals):
            return False
        intervals.append((offset, offset + width))
    return True


# Check exact wire bytes, native bits, failure atomicity and structural name matching.
def main():
    checks = 0
    with tempfile.TemporaryDirectory(prefix="dis64-storage-") as temporary:
        source = Path(temporary) / "probe.c"
        binary = Path(temporary) / "probe.so"
        source.write_text(SOURCE)
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", "-O2 -g -Wall -Wextra -Werror"))
        command += ["-std=c99", "-Wno-unused-function", "-shared", "-fPIC", "-I" + str(ROOT / "include"), str(source), "-o", str(binary)]
        subprocess.run(command, check=True)
        lib = ctypes.CDLL(str(binary))
        uintp = ctypes.POINTER(ctypes.c_uint)
        lib.dataprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(Type), ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p]
        lib.datawrite.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(Type), ctypes.POINTER(Initializer), ctypes.c_uint, uintp]
        lib.signatureprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, uintp]
        lib.signaturewrite.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, uintp, ctypes.c_uint, uintp]
        lib.signatureequal.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_int)]
        lib.bindingsprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint,
                                     ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, uintp]
        lib.bindingswrite.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(Binding), ctypes.c_uint,
                                     ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint, uintp]
        nil = bytes.fromhex("fedcba9876543210")
        recordsets = [[], [(0,1,0x80000000)], [(4,4,1)], [(8,2,0xffffffffffffffff)],
                      [(16,3,0x8000000000000000)], [(16,3,0x7ff8000000001234)],
                      [(0,4,2)], [(1,1,42)], [(0,1,1),(0,1,1)],
                      [(16,2,0x8000000000000000),(0,1,40),(4,4,1),(24,3,0x7ff0000000000000)]]
        for size in (0,4,8,16,32,64):
            for alignment in (1,2,3,4,8):
                for firstbyte in (0,0x40,0x80,1):
                    bitmap = bytes([firstbyte]) if size else b""
                    maps = ctypes.create_string_buffer(bitmap)
                    typeinfo = Type(size, alignment, ctypes.addressof(maps), len(bitmap))
                    for records in recordsets:
                        expected = struct.pack(">I", len(records))
                        for offset, kind, bits in records:
                            width = 8 if kind in (2,3) else 4
                            expected += struct.pack(">II", offset, kind) + bits.to_bytes(width,"big")
                        valid = init_valid(size, alignment, bitmap, records)
                        inputs = (Initializer * len(records))(*(Initializer(offset,kind,bits>>32,bits&0xffffffff) for offset,kind,bits in records))
                        for capacity in (0,len(expected)-1,len(expected),len(expected)+1):
                            out = ctypes.create_string_buffer(b"\xa5"*(len(expected)+4))
                            written = ctypes.c_uint(12345)
                            accepted = valid and capacity >= len(expected)
                            ok = lib.datawrite(out,capacity,ctypes.byref(typeinfo),inputs,len(records),ctypes.byref(written))
                            wanted = expected+b"\xa5"*4+b"\0" if accepted else b"\xa5"*(len(expected)+4)+b"\0"
                            if ok != int(accepted) or out.raw != wanted or written.value != (len(expected) if accepted else 12345):
                                raise RuntimeError("initializer writer mismatch")
                            checks += 1
                        for capacity in (max(size-1,0),size,size+1):
                            out = ctypes.create_string_buffer(b"\xa5"*(size+8))
                            accepted = valid and capacity >= size
                            ok = lib.dataprobe(expected,len(expected),ctypes.byref(typeinfo),out,capacity,nil)
                            wanted = bytearray(b"\xa5"*(size+8)+b"\0")
                            if accepted:
                                wanted[:size] = bytes(size)
                                for bit in range(len(bitmap)*8):
                                    if bitmap[bit//8] & (128>>(bit%8)):
                                        wanted[bit*8:bit*8+8] = nil
                                for offset,kind,bits in records:
                                    width = 8 if kind in (2,3) else 4
                                    wanted[offset:offset+width] = bits.to_bytes(width,sys.byteorder)
                            if ok != (1 if accepted else 2 if valid else 0) or out.raw != wanted:
                                raise RuntimeError("native initializer bits/nil/atomicity mismatch")
                            checks += 1
        maps = ctypes.create_string_buffer(bytes([0x40]))
        typeinfo = Type(32,8,ctypes.addressof(maps),1)
        records = [(0,1,40),(16,3,0x7ff8000000001234)]
        full = struct.pack(">I",len(records))
        for offset,kind,bits in records:
            full += struct.pack(">II",offset,kind)+bits.to_bytes(8 if kind==3 else 4,"big")
        for payload in [full[:i] for i in range(len(full))]+[full+b"\0"]:
            out = ctypes.create_string_buffer(b"\xa5"*40)
            if lib.dataprobe(payload,len(payload),ctypes.byref(typeinfo),out,32,nil)!=0 or out.raw!=b"\xa5"*40+b"\0":
                raise RuntimeError("truncated initializer changed output")
            checks+=1
        out = ctypes.create_string_buffer(b"\xa5"*40)
        if lib.dataprobe(full,len(full),ctypes.byref(typeinfo),out,32,None)!=2 or out.raw!=b"\xa5"*40+b"\0":
            raise RuntimeError("missing runtime nil was not atomic")
        checks+=1
        signatures = []
        for result in range(6):
            for parameters in ([],[0],[1],[2,3,4],[1]*16,[5]):
                image = signature(result,parameters)
                valid = result<=4 and all(1<=kind<=4 for kind in parameters)
                params = (ctypes.c_uint*len(parameters))(*parameters)
                for capacity in (0,len(image)-1,len(image),len(image)+1):
                    out=ctypes.create_string_buffer(b"\xa5"*(len(image)+4))
                    written=ctypes.c_uint(12345)
                    accepted=valid and capacity>=len(image)
                    ok=lib.signaturewrite(out,capacity,result,params,len(parameters),ctypes.byref(written))
                    wanted=image+b"\xa5"*4+b"\0" if accepted else b"\xa5"*(len(image)+4)+b"\0"
                    if ok!=int(accepted) or out.raw!=wanted or written.value!=(len(image) if accepted else 12345):
                        raise RuntimeError("signature writer mismatch")
                    checks+=1
                for payload in [image]+[image[:i] for i in range(len(image))]+[image+b"\0",signature(result,parameters,2)]:
                    fields=(ctypes.c_uint*2)(12345,12345)
                    accepted=valid and payload==image
                    if lib.signatureprobe(payload,len(payload),fields)!=int(accepted) or list(fields)!=([result,len(parameters)] if accepted else [12345]*2):
                        raise RuntimeError("signature framing/publication mismatch")
                    checks+=1
                if valid:
                    signatures.append(image)
        for left in signatures:
            for right in signatures:
                equal=ctypes.c_int(17)
                if lib.signatureequal(left,len(left),right,len(right),ctypes.byref(equal))!=1 or equal.value!=int(left==right):
                    raise RuntimeError("structural signature equality mismatch")
                checks+=1
        names=["M","f","f","g","N","M"]
        pool=pool_image(names)
        types=bytes.fromhex("00000001 00000040 00000008 00")
        sig=signature(1,[1])
        other=signature(2,[1])
        cases=[(0,[(0,1,0,0,sig)],True),(1,[(0,1,0,0,sig)],True),
               (1,[(0,1,2,0,sig)],False),(1,[(0,1,0,1,sig)],False),
               (1,[(0,1,0,0,sig),(0,2,1,0,sig)],False),
               (0,[(0,1,0,0,sig),(5,2,0,0,sig)],True),
               (0,[(0,1,0,0,sig),(5,2,0,0,other)],False),
               (0,[(0,1,0,0,sig),(4,2,0,0,other)],True),
               (0,[(99,1,0,0,sig)],False),(1,[(0,99,0,0,sig)],False),
               (0,[],True),(1,[],True)]
        for exports,records,valid in cases:
            image=struct.pack(">I",len(records))
            keep=[]
            bindings=[]
            for module,symbol,entry,frame,sigbytes in records:
                image+=(struct.pack(">IIII",symbol,entry,frame,len(sigbytes)) if exports else
                        struct.pack(">III",module,symbol,len(sigbytes)))+sigbytes
                keep.append(ctypes.create_string_buffer(sigbytes))
                bindings.append(Binding(module,symbol,entry,frame,Signature(ctypes.addressof(keep[-1]),len(sigbytes),0,0)))
            inputs=(Binding*len(bindings))(*bindings)
            for capacity in (0,len(image)-1,len(image),len(image)+1):
                out=ctypes.create_string_buffer(b"\xa5"*(len(image)+4))
                written=ctypes.c_uint(12345)
                accepted=valid and capacity>=len(image)
                ok=lib.bindingswrite(out,capacity,exports,inputs,len(records),pool,len(pool),types,len(types),ctypes.byref(written))
                wanted=image+b"\xa5"*4+b"\0" if accepted else b"\xa5"*(len(image)+4)+b"\0"
                if ok!=int(accepted) or out.raw!=wanted or written.value!=(len(image) if accepted else 12345):
                    raise RuntimeError("binding writer/context mismatch")
                checks+=1
            for payload in [image]+[image[:i] for i in range(len(image))]+[image+b"\0"]:
                fields=(ctypes.c_uint*6)(*([12345]*6))
                accepted=valid and payload==image
                want=1 if accepted and records else 2 if accepted else 0
                ok=lib.bindingsprobe(payload,len(payload),exports,pool,len(pool),types,len(types),0,fields)
                if ok!=want or (want!=1 and list(fields)!=[12345]*6):
                    raise RuntimeError("binding reader/getter atomicity mismatch")
                if want==1:
                    module,symbol,entry,frame,sigbytes=records[0]
                    _,resultkind,parameters=struct.unpack(">III",sigbytes[:12])
                    expected=[0 if exports else module,symbol,entry if exports else 0,
                              frame if exports else 0,resultkind,parameters]
                    if list(fields)!=expected:
                        raise RuntimeError("binding getter field mismatch")
                checks+=1
    print("PASS Dis64 typed storage:",checks,"initializer/native-bit/nil/signature/binding cases")


if __name__ == "__main__":
    main()
