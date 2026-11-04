#!/usr/bin/env python3
"""Independent Dis64 module-wide storage inspection; never executable admission."""
import argparse
import json
from pathlib import Path
import struct

DEFAULT_LIMITS = (64, 65536, 1048576, 65536, 65536, 1024, 256, 1024)  # Local resource policy, not wire IDs.


# A bounded cursor never pads a truncated record or reads outside its payload.
class Cursor:


    # Retain one immutable byte span and its current consumption offset.
    def __init__(self, data):
        self.data = data  # Immutable payload bytes, independent of native layouts.
        self.at = 0  # Next unread byte, always within the payload.


    # Consume an exact byte span before any typed decoding or count-driven loop.
    def take(self, size):
        if size < 0 or size > len(self.data)-self.at:
            raise ValueError('truncated payload')
        start = self.at
        self.at += size
        return self.data[start:self.at]


    # Decode a single bounded big-endian unsigned word.
    def word(self):
        return int.from_bytes(self.take(4), 'big')


    # Bound population by both caller policy and minimum serialized record size.
    def count(self, limit, minimum):
        count = self.word()
        if count > limit or count > (len(self.data)-self.at)//minimum:
            raise ValueError('record population')
        return count


    # Require exact consumption, including empty tables and count-zero payloads.
    def finish(self):
        if self.at != len(self.data):
            raise ValueError('payload trailer')


# Inspect the common container without interpreting unknown optional payloads.
def envelope(data, section_limit):
    if len(data) < 32 or len(data) > 0xffffffff or data[:8] != b'Dis64\r\n\x1a':
        raise ValueError('container header')
    major, minor, header, abi, flags, extensions, count = struct.unpack('>HHIIIII', data[8:32])
    if (major, minor, abi, flags) != (1, 0, 1, 0) or extensions != 0:
        raise ValueError('unsupported container version, ABI, flags or mandatory extension')
    if count > section_limit or header != 32+12*count or header > len(data):
        raise ValueError('container directory')
    cursor = header
    seen = set()
    sections = {}
    for index in range(count):
        kind, flags, offset, size = struct.unpack('>HHII', data[32+12*index:44+12*index])
        if offset != cursor or size > len(data)-cursor:
            raise ValueError('section extent')
        known = 1 <= kind <= 8
        singleton = known or kind in (32768, 32769)
        if flags != (1 if known else 0) or (singleton and kind in seen):
            raise ValueError('section flags or duplicate')
        if (kind in (1, 2, 3) and size == 0) or (5 <= kind <= 8 and size < 4):
            raise ValueError('section minimum extent')
        seen.add(kind)
        if known:
            sections[kind] = data[offset:offset+size]
        cursor += size
    if cursor != len(data) or not {1, 2, 3, 7}.issubset(sections) or 8 in sections:
        raise ValueError('required storage sections or unsupported CONTROL')
    return sections


# Validate every descriptor/map, including descriptors unreachable from module data.
def type_table(payload, count_limit, object_limit):
    reader = Cursor(payload)
    descriptors = []
    for unused in range(reader.count(count_limit, 8)):
        size, align = reader.word(), reader.word()
        if size > object_limit or align not in (1, 2, 4, 8) or size % align or (size == 0 and align != 1):
            raise ValueError('type geometry')
        bitmap = reader.take((size+63)//64)
        if align < 8 and any(bitmap):
            raise ValueError('mapped type alignment')
        for byte_index, byte in enumerate(bitmap):
            for bit in range(8):
                if byte & (128 >> bit) and (byte_index*8+bit)*8+8 > size:
                    raise ValueError('mapped reference extent')
        descriptors.append((size, align, bitmap))
    reader.finish()
    return tuple(descriptors)


# Preserve every Rune32 payload bit; content-equal records need not share an ordinal.
def string_pool(payload, limit):
    reader = Cursor(payload)
    strings = []
    for unused in range(reader.count(limit, 4)):
        count = reader.count(0xffffffff, 4)
        strings.append(tuple(int.from_bytes(reader.take(4), 'big') for unused in range(count)))
    reader.finish()
    return tuple(strings)


# Resolve an ordinal without Python negative-index aliases or synthesized absent values.
def ordinal(table, index):
    if index < 0 or index >= len(table):
        raise ValueError('missing ordinal')
    return table[index]


# Decode only framing of signed7/14/30, without assigning arithmetic or call semantics.
def operand(reader):
    first = reader.take(1)[0]
    width = 1 if first < 128 else 2 if first < 192 else 4
    bits = first & (127 if width == 1 else 63)
    for byte in reader.take(width-1):
        bits = bits*256+byte
    precision = {1: 7, 2: 14, 4: 30}[width]
    return bits-(1 << precision) if bits & (1 << (precision-1)) else bits


# Validate direct-mode instruction extents and existing ISA range, not opcode roles.
def code_table(payload, limit):
    reader = Cursor(payload)
    count = reader.count(limit, 2)
    for unused in range(count):
        opcode, mode = reader.take(2)
        source, destination, middle = (mode >> 3) & 7, mode & 7, mode >> 6
        if opcode >= 175 or source > 3 or destination > 3:
            raise ValueError('unsupported opcode range or indirect framing')
        if middle:
            operand(reader)
        if source != 3:
            operand(reader)
        if destination != 3:
            operand(reader)
    reader.finish()
    return count


# Check all scalar DATA writes against actual MP geometry and protected ref64 slots.
def initializers(payload, descriptor, limit):
    reader = Cursor(payload)
    size, align, bitmap = descriptor
    spans = []
    for unused in range(reader.count(limit, 12)):
        offset, kind = reader.word(), reader.word()
        width = {1: 4, 2: 8, 3: 8, 4: 4}.get(kind)
        if width is None or offset % width or offset+width > size:
            raise ValueError('DATA scalar extent or kind')
        value = int.from_bytes(reader.take(width), 'big')
        if kind == 4 and value > 1:
            raise ValueError('DATA Bool value')
        slot = offset//8
        if bitmap[slot//8] & (128 >> (slot % 8)):
            raise ValueError('DATA mapped reference overlap')
        if any(offset < end and begin < offset+width for begin, end in spans):
            raise ValueError('DATA overlap')
        spans.append((offset, offset+width))
    reader.finish()
    return len(spans)


# Keep callable signatures versioned and exact; Bool is a legal scalar signature kind.
def signature(payload, limit):
    reader = Cursor(payload)
    version, result = reader.word(), reader.word()
    count = reader.count(limit, 4)
    if version != 1 or result not in (0, 1, 2, 3, 4):
        raise ValueError('signature version or result')
    parameters = tuple(reader.word() for unused in range(count))
    if any(kind not in (1, 2, 3, 4) for kind in parameters):
        raise ValueError('signature parameter kind')
    reader.finish()
    return result, parameters


# Join pool names, export PCs/frame shapes and exact same-name import signatures.
def bindings(payload, exports, strings, types, instructions, count_limit, parameter_limit):
    reader = Cursor(payload)
    records = {}
    count = reader.count(count_limit, 28 if exports else 24)
    for unused in range(count):
        first, second = reader.word(), reader.word()
        if exports:
            symbol, entry, frame = first, second, reader.word()
            frame_size, frame_align, bitmap = ordinal(types, frame)
            if entry >= instructions or frame_size < 64 or frame_align != 8:
                raise ValueError('export entry or frame')
            key = ordinal(strings, symbol)
        else:
            key = ordinal(strings, first), ordinal(strings, second)
        size = reader.word()
        callable_type = signature(reader.take(size), parameter_limit)
        if key in records and (exports or records[key] != callable_type):
            raise ValueError('duplicate export or conflicting import')
        records[key] = callable_type
    reader.finish()
    return count


# Join structural module storage only; no call graph, initialization-flow or owning execution grant.
def inspect_storage(data, limits=DEFAULT_LIMITS):
    if not isinstance(data, bytes):
        raise TypeError('storage input must be immutable bytes')
    if len(limits) != 8 or any(type(value) is not int or not 0 <= value <= 0xffffffff for value in limits):
        raise ValueError('resource policy')
    section_limit, type_limit, object_limit, string_limit, code_limit, binding_limit, parameter_limit, data_limit = limits
    sections = envelope(data, section_limit)
    module = Cursor(sections[1])
    name, profile, size = module.word(), module.word(), module.word()
    if profile not in (0, 1) or size > object_limit:
        raise ValueError('module profile or data extent')
    datatype = module.word() if size else None
    entry, frame = (module.word(), module.word()) if profile else (None, None)
    module.finish()
    types = type_table(sections[3], type_limit, object_limit)
    strings = string_pool(sections[7], string_limit)
    ordinal(strings, name)
    instructions = code_table(sections[2], code_limit)
    if profile:
        frame_size, frame_align, bitmap = ordinal(types, frame)
        if entry >= instructions or frame_size < 64 or frame_align != 8:
            raise ValueError('module entry or frame')
    descriptor = ordinal(types, datatype) if size else (0, 1, b'')
    if descriptor[0] != size:
        raise ValueError('module data type extent')
    initialized = initializers(sections[4], descriptor, data_limit) if 4 in sections else None
    imports = bindings(sections[5], False, strings, types, instructions, binding_limit, parameter_limit) if 5 in sections else None
    exports = bindings(sections[6], True, strings, types, instructions, binding_limit, parameter_limit) if 6 in sections else None
    return dict(profile=profile, databytes=size, datatype=datatype, entry=entry, frametype=frame,
                types=len(types), strings=len(strings), instructions=instructions,
                initializers=initialized, imports=imports, exports=exports,
                owning_execution_supported=False)


# Offer an explicit diagnostic command; local refusal reasons are not new shared wire error IDs.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    if args.image.stat().st_size > 8*1024*1024:
        parser.error('local inspection limit: 8 MiB')
    try:
        result = inspect_storage(args.image.read_bytes())
    except ValueError as error:
        print(json.dumps(dict(storage_checked=False, reason=str(error), owning_execution_supported=False)))
        return 1
    print(json.dumps(dict(storage_checked=True, **result), sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
