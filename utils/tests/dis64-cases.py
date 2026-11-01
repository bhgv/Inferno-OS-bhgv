#!/usr/bin/env python3
"""Compile bounded numeric cases, independently check results and old/new control-flow identity."""
from pathlib import Path
import argparse
import copy
import json
import os
import re
import runpy
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
EXECUTE = runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))['execute']
PREFIX = 'implement CaseProbe;\nCaseProbe: module {init: fn();};\n'


# Render exact signed endpoint bits without passing through host signed narrowing.
def literal(value, big=False):
    return ('big' if big else 'int')+' 16r'+format(value & ((1 << (64 if big else 32))-1), 'x')


# Keep each selector global/live; the expected arm is evaluated independently in Python.
def programs():
    families = (
        ('sparse', False, [(-100,-91,21),(10,20,31),(1000000,1000003,41)],
         [-101,-100,-95,-91,-90,0,9,10,15,20,21,999999,1000000,1000003,1000004]),
        ('dense', False, [(-1,-1,21),(0,0,31),(1,1,41)], [-3,-2,-1,0,1,2,3]),
        ('dense-gaps', False, [(4,4,21),(6,6,31),(8,8,41)], [3,4,5,6,7,8,9]),
        ('word-extrema', False, [(-(1<<31),-(1<<31)+2,21),(-7,7,31),((1<<31)-2,(1<<31)-1,41)],
         [-(1<<31),-(1<<31)+1,-(1<<31)+2,-(1<<31)+3,-8,-7,0,7,8,(1<<31)-3,(1<<31)-2,(1<<31)-1]),
        ('big-extrema', True, [(-(1<<63),-(1<<63),21),(-7,-7,31),(7,7,32),((1<<63)-3,(1<<63)-3,41),((1<<63)-1,(1<<63)-1,42)],
         [-(1<<63),-(1<<63)+1,-(1<<63)+2,-(1<<63)+3,-8,-7,0,7,8,(1<<63)-4,(1<<63)-3,(1<<63)-1]),
        ('big-wide', True, [(-(1<<40),-(1<<40),21),((1<<39),(1<<39),31)],
         [-(1<<40)-1,-(1<<40),-(1<<40)+2,-(1<<40)+3,0,(1<<39)-1,1<<39,(1<<39)+3,(1<<39)+4]),
    )
    for family, big, arms, values in families:
        labels = []
        for lo, hi, result in arms:
            qualifier = literal(lo,big)+((' to '+literal(hi,big)) if hi!=lo else '')
            labels.append(qualifier+' => answer = '+str(result)+';')
        for value in values:
            expected = next((result for lo,hi,result in arms if lo<=value<=hi),77)
            text = PREFIX+'key := '+literal(value,big)+'; answer: int;\ninit(){\n case key {\n'+ '\n'.join(labels)+'\n * => answer = 77;\n }\n if(answer != '+str(expected)+') answer /= 0;\n}\n'
            yield family+'-'+str(value),text,''
    yield 'nested', PREFIX+'''key := 17; inner := big 1099511627776; answer: int;
init(){case key {1 => answer=1; 10 to 20 => case inner {big 1 => answer=2; big 1099511627776 => answer=99; * => answer=3;} * => answer=4;} if(answer!=99) answer/=0;}
''',''
    yield 'loop', PREFIX+'''counter: int;
init(){for(i:=0;i<4097;i++){case i%3 {0 => counter+=1; 1 => counter+=2; 2 => counter+=3; * => counter/=0;}} if(counter!=8193) counter/=0;}
''',''
    yield 'expression', PREFIX+'''left:=12345; right:=789; answer:int;
init(){a:=big 17;b:=big 33;case (left^right)&255 {0 => answer=1; 44 => answer=99; * => answer=3;} if(answer!=99 || a!=big 17 || b!=big 33) answer/=0;}
''',''
    yield 'implicit-default', PREFIX+'''key:=999; answer:=77;
init(){case key {1 => answer=1; 10 to 20 => answer=2;} if(answer!=77) answer/=0;}
''',''
    yield 'wrong-answer', PREFIX+'''key:=17; answer:int;
init(){case key {1 => answer=11; 10 to 20 => answer=99; * => answer=77;} if(answer!=98) answer/=0;}
''','zero divide'


# Bound test subprocesses and preserve real compiler diagnostics on failure.
def command(arguments, accepted=True):
    result = subprocess.run(list(map(str, arguments)), capture_output=True, timeout=30)
    if (result.returncode == 0) != accepted:
        raise RuntimeError(repr(arguments) + '\n' + result.stdout.decode(errors='replace') + result.stderr.decode(errors='replace'))
    return result.stdout


# Decode code and signed30 operands independently of the C backend and role validator.
def decoded(image):
    sections = struct.unpack_from('>I', image, 28)[0]
    for index in range(sections):
        kind, flags, offset, size = struct.unpack_from('>HHII', image, 32 + 12 * index)
        if kind == 2:
            code = image[offset:offset + size]
            break
    else:
        raise RuntimeError('Missing code section')
    count = struct.unpack_from('>I', code)[0]
    at = 4
    rows = []
    for unused in range(count):
        op, mode = code[at:at + 2]
        at += 2
        args = []
        for present in (bool(mode & 0xc0), ((mode >> 3) & 7) != 3, (mode & 7) != 3):
            if not present:
                continue
            first = code[at]
            length = 1 if first < 128 else 2 if first < 192 else 4
            value = first & (127 if length == 1 else 63)
            for byte in code[at + 1:at + length]:
                value = value << 8 | byte
            bits = 7 if length == 1 else 14 if length == 2 else 30
            if value & (1 << (bits-1)):
                value -= 1 << bits
            args.append(value)
            at += length
        rows.append(dict(op=op, mode=mode, args=args))
    if at != len(code):
        raise RuntimeError('Bad instruction framing')
    return rows


# Derive expansion geometry from the ordinary assembler's real case-table counts.
def branches(assembly, rows):
    instructions = assembly.split('\tentry\t', 1)[0].splitlines()
    instructions = [line.strip() for line in instructions if line.startswith('\t')]
    tables = {}
    for match in re.finditer('\\tword\\t@mp(?:\\+(\\d+))?,(-?\\d+)', assembly):
        tables[int(match[1] or 0)] = int(match[2])
    mapping = []
    expanded = 0
    for line in instructions:
        mapping.append(expanded)
        op = line.split()[0]
        if op in ('case', 'casel', 'goto'):
            offset = int(re.search(',(\\d+)\\(mp\\)$', line)[1]) - (4 if op == 'goto' else 0)
            expanded += 1 + (tables[offset] - 1 if op == 'goto' else 2 * tables[offset])
        else:
            expanded += 1
    if expanded != len(rows):
        raise RuntimeError('Expansion count ' + str(expanded) + ' != ' + str(len(rows)) + ' tables=' + repr(tables) + '\n' + assembly)
    witnesses = []
    for pc, line in enumerate(instructions):
        target = re.search('(?:,|\\s)\\$(\\d+)$', line)
        if target and (line.startswith('jmp') or line.startswith('b')):
            original = int(target[1])
            expected = mapping[original]
            witnesses.append((mapping[pc], original, expected))
            if rows[mapping[pc]]['args'][-1] != expected:
                raise RuntimeError('Original branch was not remapped')
    return witnesses


# Check source semantics, source maps and compatibility without admitting a file-table opcode.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--emu', type=Path)
    parser.add_argument('--runner', default='')
    parser.add_argument('--reader', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    compiler = args.limbo.resolve()
    count = processes = controls = 0
    seen = set()
    with tempfile.TemporaryDirectory(prefix='scalar cases ', dir=root) as directory:
        work = Path(directory)
        source = work / 'case.b'
        image = work / 'case.dis64'
        asm = work / 'case.s'
        legacy = work / 'old.dis'
        smap = work / 'case.json'
        for name, text, error in programs():
            source.write_text(text)
            command([compiler, '-S', '-o', asm, source])
            assembly = asm.read_text()
            seen.update(re.findall('^\\t(case|casel|goto)\\t', assembly, re.M))
            command([compiler, '-o', legacy, source])
            before = legacy.read_bytes()
            command([sys.executable, ROOT / 'utils/dis64-locations.py', 'compile', '--limbo', compiler, '--source', source, '--image', image, '--map', smap])
            rows = decoded(image.read_bytes())
            witnesses = branches(assembly, rows)
            locations = json.loads(smap.read_bytes())['instructions']
            if len(locations) != len(rows) or any((row['pc'] != pc or row['op'] != rows[pc]['op'] for pc, row in enumerate(locations))):
                raise RuntimeError('Expanded source-map identity mismatch')
            for pc, old, new in witnesses:
                if old != new:
                    mutant = copy.deepcopy(rows)
                    mutant[pc]['args'][-1] = old
                    try:
                        branches(assembly, mutant)
                    except RuntimeError:
                        controls += 1
                    else:
                        raise RuntimeError('Stale-PC control escaped')
                    break
            if args.reader:
                command([args.reader.resolve(), image, '--pilot'])
            command([compiler, '-o', legacy, source])
            if legacy.read_bytes() != before:
                raise RuntimeError('Ordinary Dis changed')
            if args.emu:
                for jit in (0, 1):
                    output = EXECUTE(shlex.split(args.runner) + [str(args.emu.resolve()), '-v', '-r', str(root), '-c', str(jit), '-d', '/' + str(image.relative_to(root))])
                    marker = 'native64 scalar module CaseProbe: jit=' + str(jit) + ', exit="' + error + '"'
                    if marker not in output or ('Broken:' in output) != bool(error) or 'native64 scalar scheduler empty after program retirement' not in output or ('panic:' in output) or ('faults:' in output):
                        raise RuntimeError(name + ' JIT=' + str(jit) + '\n' + output)
                    if error:
                        fault = re.findall(r'native64 scalar fault: dis-pc=(\d+)', output)
                        expected_line = next(index+1 for index, line in enumerate(text.splitlines()) if 'answer!=98' in line)
                        if len(fault) != 1 or locations[int(fault[0])]['start']['line'] != expected_line:
                            raise RuntimeError('Fault source identity was lost after case expansion')
                    processes += 1
            count += 1
        for text in (PREFIX + 'key: string; init(){case key {"x"=>key="y"; * =>key="z";}}', PREFIX + 'key:=0; init(){case key {' + ' '.join((str(i * 100) + '=>key=1;' for i in range(9))) + ' * =>key=2;}}'):
            source.write_text(text)
            command([compiler, '-o', legacy, source])
            command([compiler, '-6', '-o', image, source], False)
            if image.exists():
                raise RuntimeError('Refused case left output')
    if seen != {'case', 'casel', 'goto'} or controls == 0:
        raise RuntimeError('Missing real case forms or stale-PC controls: ' + repr(seen))
    print('PASS scalar cases:', count, 'sources,', processes, 'whole C/JIT processes,', controls, 'active stale-PC controls; ordinary Dis unchanged; string/large cases refused')


if __name__=='__main__':
    main()
