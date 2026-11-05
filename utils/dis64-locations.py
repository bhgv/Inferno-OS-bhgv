#!/usr/bin/env python3
"""Compile a scalar Dis64 image with a private source map, or resolve a checked Dis PC."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import struct
import sys
import subprocess
import tempfile

LIMIT = 32*1024*1024  # Bound diagnostic sidecars before JSON parsing.
SCHEMA = 'inferno-dis64-locations/1'  # Private tooling, not a shared Dis64 section or callable descriptor.


# Decode just instruction boundaries independently of the compiler's trace writer.
def opcodes(image):
    if len(image) < 32 or image[:8] != b'Dis64\r\n\x1a':
        raise ValueError('Expected a Dis64 image')
    count = struct.unpack_from('>I', image, 28)[0]
    if count > (len(image)-32)//12:
        raise ValueError('Truncated section directory')
    code = None
    for index in range(count):
        kind, flags, offset, size = struct.unpack_from('>HHII', image, 32+12*index)
        if offset > len(image) or size > len(image)-offset:
            raise ValueError('Truncated section')
        if kind == 2:
            if code is not None:
                raise ValueError('Duplicate code section')
            code = image[offset:offset+size]
    if code is None or len(code) < 4:
        raise ValueError('Missing code section')
    count = struct.unpack_from('>I', code)[0]
    if count > 65536:
        raise ValueError('Outside scalar instruction limit')
    cursor, result = 4, []
    for unused in range(count):
        if cursor+2 > len(code):
            raise ValueError('Truncated instruction')
        op, mode = code[cursor:cursor+2]
        cursor += 2
        if ((mode >> 3) & 7) > 3 or (mode & 7) > 3:
            raise ValueError('Outside scalar addressing profile')
        operands = (mode & 0xc0 != 0)+(((mode >> 3) & 7) != 3)+((mode & 7) != 3)
        for unused in range(operands):
            if cursor >= len(code):
                raise ValueError('Truncated operand')
            cursor += 1 if code[cursor] < 0x80 else 2 if code[cursor] < 0xc0 else 4
            if cursor > len(code):
                raise ValueError('Truncated operand')
        result.append(op)
    if cursor != len(code):
        raise ValueError('Trailing code bytes')
    return result


# Check every PC/opcode pair; locations describe source text and confer no runtime authority.
def validate(document, image):
    if not isinstance(document, dict):
        raise ValueError('Expected a source map object')
    if document.get('schema') != SCHEMA or document.get('image_sha256') != hashlib.sha256(image).hexdigest():
        raise ValueError('Source map does not match this exact image')
    rows, ops = document.get('instructions'), opcodes(image)
    if not isinstance(rows, list) or len(rows) != len(ops):
        raise ValueError('Incomplete instruction map')
    for pc, (row, op) in enumerate(zip(rows, ops)):
        if (not isinstance(row, dict) or type(row.get('pc')) is not int or row['pc'] != pc or
                type(row.get('op')) is not int or row['op'] != op or 'start' not in row or 'stop' not in row):
            raise ValueError('Mismatched instruction identity')
        for key in ('start', 'stop'):
            loc = row.get(key)
            if loc is not None and (not isinstance(loc, dict) or not isinstance(loc.get('file'), str) or
                    type(loc.get('line')) is not int or type(loc.get('column')) is not int or
                    loc['line'] < 0 or loc['column'] < 0):
                raise ValueError('Invalid source location')
    return rows


# Read one opened input with an actual byte bound, including files that grow
# after metadata inspection. Borrow path; own/close the stream and returned bytes.
# Reading at most LIMIT+1 detects excess without materializing the entire input.
def read(path):
    with path.open('rb') as stream:
        payload = stream.read(LIMIT + 1)
    if len(payload) > LIMIT:
        raise ValueError('Diagnostic input exceeds size limit')
    return payload


# Stage a sidecar next to its destination; failed compilation or validation never replaces it.
def compile_map(args):
    source, image, target = (p.resolve() for p in (args.source, args.image, args.map))
    paths = (source, image, target)
    if len(set(paths)) != 3 or any(a.exists() and b.exists() and a.samefile(b)
            for i, a in enumerate(paths) for b in paths[i+1:]):
        raise ValueError('Source, image and source map must be different files')
    env = dict(os.environ, LIMBO_DIS64_TRACE='1')
    result = subprocess.run([str(args.limbo.resolve()), '-64', '-o', str(image), str(source)],
                            env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise ValueError(result.stderr.decode(errors='replace'))
    if len(result.stderr) > LIMIT:
        raise ValueError('Compiler trace exceeds size limit')
    for line in result.stderr.splitlines(keepends=True):
        if not line.startswith(b'LIMBO64LOC '):
            sys.stderr.buffer.write(line)
    rows = [json.loads(line[len(b'LIMBO64LOC '):]) for line in result.stderr.splitlines()
            if line.startswith(b'LIMBO64LOC ')]
    encoded = read(image)
    document = dict(schema=SCHEMA, image_sha256=hashlib.sha256(encoded).hexdigest(), instructions=rows)
    validate(document, encoded)
    payload = (json.dumps(document, ensure_ascii=True, indent=2)+'\n').encode('ascii')
    if len(payload) > LIMIT:
        raise ValueError('Source map exceeds diagnostic size limit')
    with tempfile.NamedTemporaryFile(dir=target.parent, prefix=target.name+'.', delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(payload)
            stream.close()
            temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    print('Mapped', len(rows), 'Dis instructions:', target)


# Resolve without reading or executing any file path named by the diagnostic map.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    compile_ = commands.add_parser('compile')
    compile_.add_argument('--limbo', type=Path, required=True)
    compile_.add_argument('--source', type=Path, required=True)
    for sub in (compile_, commands.add_parser('lookup')):
        sub.add_argument('--image', type=Path, required=True)
        sub.add_argument('--map', type=Path, required=True)
    commands.choices['lookup'].add_argument('--pc', type=int, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'compile':
            compile_map(args)
        else:
            rows = validate(json.loads(read(args.map)), read(args.image))
            if args.pc < 0 or args.pc >= len(rows):
                raise ValueError('Dis PC outside image')
            print(json.dumps(rows[args.pc], ensure_ascii=True))
    except (OSError, ValueError, TypeError, KeyError, struct.error) as error:
        parser.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
