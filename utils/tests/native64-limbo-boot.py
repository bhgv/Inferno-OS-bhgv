#!/usr/bin/env python3
"""Compile actual Limbo sources into the agreed scalar Dis64 profile and execute whole emulators."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PREFIX = 'implement FromLimbo;\nFromLimbo: module { init: fn(); };\n'


# Keep arithmetic variables live so the VM, rather than constant folding, proves the result.
def sources():
    yield 'empty', '', 'init() {}', ''
    yield 'word', 'a := 2147483647;', 'init(){ a += 1; if(a != int 16r80000000) a /= 0; }', ''
    yield 'big', 'a := big 16r7fffffffffffffff;', 'init(){ a += big 1; if(a != big 16r8000000000000000) a /= big 0; }', ''
    yield 'big-division', 'a := big 16r8000000000000000; b := big 16r8000000000000000;', 'init(){ a /= big -1; b %= big -1; if(a != big 16r8000000000000000 || b != big 0) a /= big 0; }', ''
    yield 'signed-division', 'a := -17; b := -17;', 'init(){ a /= 3; b %= 3; if(a != -5 || b != -2) a /= 0; }', ''
    yield 'bitwise', 'a := 12345; b := 789;', 'init(){ a = (a ^ b) & 16rffff; a |= 16r10000; if(a != 78636) a /= 0; }', ''
    yield 'scratch-alias', 'a := 12345; b := 789; answer: int;', 'init(){ x := big 17; y := big 33; z := big 55; answer = ((a ^ b) & 16rffff) | 16r10000; if(x != big 17 || y != big 33 || z != big 55 || answer != 78636) answer /= 0; }', ''
    yield 'shifts', 'a := big -1; b := -1;', 'init(){ a >>= 63; b >>= 31; if(a != big -1 || b != -1) b /= 0; a <<= 63; if(a != big 16r8000000000000000) b /= 0; }', ''
    yield 'locals', 'answer: int;', 'init(){ a := 7; b := 11; x := big 33; y := big 10; r := 1.25; s := r; answer = a*b+3; x = (x ^ y)+big 2; if(answer != 80 || x != big 45 || r != s) answer /= 0; }', ''
    yield 'real-initializer', 'r := 3.5; s := -2.25; a: int;', 'init(){ t := r; r = s; if(t != 3.5 || r != -2.25) a /= 0; }', ''
    yield 'quantum', 'counter: int;', 'init(){ counter = 0; for(i := 0; i < 4097; i++) counter++; if(counter != 4097) counter /= 0; }', ''
    yield 'while', 'counter: int;', 'init(){ counter = 17; while(counter > 0) counter--; if(counter != 0) counter /= 0; }', ''
    yield 'zero-divide', 'a := 17;', 'init(){ a /= 0; }', 'zero divide'
    yield 'bad-shift', 'a := 17;', 'init(){ a <<= 32; }', 'invalid argument'
    # A wrong expected answer must enter the actual VM error/retirement path.
    yield 'wrong-answer', 'a := 41;', 'init(){ a++; if(a != 43) a /= 0; }', 'zero divide'


# Validate format/layout independently of the compiler's C writers and final validator.
def inspect(data):
    if data[:8] != b'Dis64\r\n\x1a' or struct.unpack_from('>HHIIIII', data, 8) != (1, 0, 92, 1, 0, 0, 5):
        raise RuntimeError('Compiler did not emit the established extension-free Dis64 envelope')
    sections = {}
    end = 92
    for i, expected in enumerate((1, 2, 3, 4, 7)):
        kind, flags, offset, size = struct.unpack_from('>HHII', data, 32+12*i)
        if kind != expected or flags != 1 or offset != end or size > len(data)-offset:
            raise RuntimeError('Unexpected section geometry')
        sections[kind] = data[offset:offset+size]
        end += size
    if end != len(data):
        raise RuntimeError('Trailing or missing compiler output')
    module = struct.unpack('>'+'I'*(len(sections[1])//4), sections[1])
    if module[:2] != (0, 1) or module[-2:] != (0, 1):
        raise RuntimeError('Unexpected executable entry/module metadata')
    types = sections[3]
    if struct.unpack_from('>I', types)[0] != 2:
        raise RuntimeError('Unexpected descriptor count')
    at = 4
    for index in range(2):
        size, alignment = struct.unpack_from('>II', types, at)
        at += 8
        count = (size+63)//64
        if any(types[at:at+count]) or len(types[at:at+count]) != count:
            raise RuntimeError('Scalar compiler output contains a managed map')
        if index == 0 and (size != module[2] or alignment != (4 if size else 1)):
            raise RuntimeError('Module data descriptor mismatch')
        if index == 1 and (size < 64 or size % 8 or alignment != 8):
            raise RuntimeError('Entry frame did not reserve the native64 prefix')
        at += count
    if at != len(types):
        raise RuntimeError('Unexpected descriptor trailing bytes')
    return struct.unpack_from('>I', sections[2])[0]


# Negative sources must first compile as ordinary Limbo, excluding syntax errors as fake refusals.
def refusals():
    yield 'string', PREFIX+'s := "hello"; init(){ if(len s > 0) s = nil; }'
    yield 'array', PREFIX+'a: array of int; init(){ a = array[2] of int; }'
    yield 'byte', PREFIX+'b := byte 3; init(){ b += byte 1; }'
    yield 'real-arithmetic', PREFIX+'r := 1.5; init(){ r += 1.5; }'
    yield 'call-arguments', PREFIX+'a: int; helper(x:int){a+=x;} init(){helper(7);}'
    yield 'arguments', 'implement FromLimbo; FromLimbo:module{init:fn(a:int);}; init(a:int){a++;}'
    yield 'result', 'implement FromLimbo; FromLimbo:module{init:fn():int;}; init():int{return 3;}'
    yield 'library', 'implement FromLimbo; FromLimbo:module{run:fn();}; run(){}'
    yield 'export', 'implement FromLimbo; FromLimbo:module{init:fn(); extra:fn();}; init(){} extra(){}'
    yield 'global-export', 'implement FromLimbo; FromLimbo:module{init:fn(); a:int;}; init(){a=3;}'


# Preserve legacy products, require failed-output cleanup, then execute real OS processes when supplied.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--limbo-runner', default='', help='explicit executor for the compiler, independent of the VM runner')
    parser.add_argument('--reference-limbo', type=Path, help='native compiler used for cross-architecture byte identity')
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--emu', type=Path)
    parser.add_argument('--runner', default='')
    args = parser.parse_args()
    limbo = shlex.split(args.limbo_runner)+[str(args.limbo.resolve())]
    root = args.root.resolve()
    execute = runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))['execute']
    retired = 'native64 scalar scheduler empty after program retirement'
    env = dict(os.environ)
    compilations = processes = 0
    with tempfile.TemporaryDirectory(prefix='limbo64 source and output ', dir=root) as directory:
        work = Path(directory)
        for name, globals_, body, error in sources():
            source = work/(name+' source.b'); source.write_text(PREFIX+globals_+'\n'+body+'\n')
            legacy, image = work/'legacy.dis', work/(name+' output.dis64')
            subprocess.run(limbo+['-o', str(legacy), str(source)], env=env, check=True)
            before = legacy.read_bytes()
            subprocess.run(limbo+['-6', '-o', str(image), str(source)], env=env, check=True)
            count = inspect(image.read_bytes())
            if args.reference_limbo:
                reference = work/'reference.dis64'
                subprocess.run([str(args.reference_limbo.resolve()), '-6', '-o', str(reference), str(source)], env=env, check=True)
                if reference.read_bytes() != image.read_bytes():
                    raise RuntimeError('Target compiler changed portable Dis64 bytes: '+name)
            subprocess.run(limbo+['-o', str(legacy), str(source)], env=env, check=True)
            if before != legacy.read_bytes() or before.startswith(b'Dis64'):
                raise RuntimeError('Experimental output changed the legacy product')
            compilations += 1
            if args.emu:
                for jit in (0, 1):
                    command = shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root), '-c', str(jit), '-d', '/'+str(image.relative_to(root))]
                    output = execute(command)
                    marker = 'native64 scalar module FromLimbo: jit='+str(jit)+', exit="'+error+'"'
                    if marker not in output or retired not in output or ('Broken:' in output) != bool(error) or 'panic:' in output or 'faults:' in output:
                        raise RuntimeError(name+' failed JIT='+str(jit)+': '+output)
                    processes += 1
            print('PASS Limbo to Dis64:', name, count, 'instructions', flush=True)
        for name, text in refusals():
            source = work/(name+' refused.b'); source.write_text(text)
            subprocess.run(limbo+['-i', '-o', str(work/'ordinary.dis'), str(source)], env=env, check=True)
            target = work/'refused.dis64'
            result = subprocess.run(limbo+['-6', '-i', '-o', str(target), str(source)], env=env, capture_output=True, text=True)
            if result.returncode == 0 or target.exists() or 'Dis64 scalar:' not in result.stdout+result.stderr:
                raise RuntimeError('Unsupported program was not cleanly refused: '+name+'\n'+result.stdout+result.stderr)
            compilations += 1
        source = work/'default.b'; source.write_text(PREFIX+'init(){}')
        subprocess.run(limbo+['-6', str(source)], cwd=work, env=env, check=True)
        inspect((work/'default.dis64').read_bytes())
        for option in ('-S', '-H', '-a', '-g', '-c', '-C'):
            result = subprocess.run(limbo+['-6', option, str(source)], cwd=work, env=env, capture_output=True, text=True)
            if result.returncode == 0 or 'usage:' not in result.stderr:
                raise RuntimeError('Incompatible output mode was accepted: '+option)
        print('PASS scalar compiler:', compilations, 'source/refusal cases; legacy unchanged; CLI/default suffix; full OS processes='+str(processes), flush=True)


if __name__ == '__main__':
    main()
