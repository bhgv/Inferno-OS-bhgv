#!/usr/bin/env python3
"""Execute restricted source helpers without admitting a file calling ABI."""
from pathlib import Path
import argparse
import json
import re
import runpy
import shlex
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HELPERS = runpy.run_path(str(ROOT/'utils/tests/dis64-cases.py'))
PREFIX = 'implement InlineProbe;\nInlineProbe: module {init: fn();};\n'


# Retain source calls in ordinary IR while checking effects with independent constant answers.
def programs():
    yield 'before-entry', 'value:=17;', 'helper(){value+=3;}\ninit(){helper();helper();if(value!=23)value/=0;}', ''
    yield 'after-entry', 'value:=17;', 'init(){helper();if(value!=20)value/=0;}\nhelper(){value+=3;}', ''
    yield 'multiple', 'value:=17;', 'a(){value+=3;} b(){value*=2;} init(){a();b();a();if(value!=43)value/=0;}', ''
    yield 'empty', 'value:=17;', 'helper(){} init(){helper();if(value!=17)value/=0;}', ''
    yield 'loop', 'value:int;', 'helper(){value++;} init(){for(i:=0;i<4097;i++)helper();if(value!=4097)value/=0;}', ''
    yield 'branches', 'value:=17;', 'helper(){value+=3;} init(){if(value==17)helper();else value/=0;if(value!=20)value/=0;}', ''
    yield 'case', 'key:=17; value:=17;', 'helper(){value+=3;} init(){case key {1=>value/=0; 10 to 20=>helper(); * =>value/=0;} if(value!=20)value/=0;}', ''
    yield 'dense-case', 'key:=1; value:=17;', 'helper(){value+=3;} init(){case key {0=>value/=0; 1=>helper(); 2=>value/=0; * =>value/=0;} if(value!=20)value/=0;}', ''
    yield 'big-case', 'key:=big 16r7fffffffffffffff; value:=17;', 'helper(){value+=3;} init(){case key {big -1=>value/=0; big 16r7fffffffffffffff=>helper(); * =>value/=0;} if(value!=20)value/=0;}', ''
    yield 'case-loop', 'value:int;', 'helper(){value+=3;} init(){for(i:=0;i<4097;i++){case i%3 {0=>helper(); 1=>helper(); 2=>helper(); * =>value/=0;}} if(value!=12291)value/=0;}', ''
    yield 'big-values', 'value:=big 16r7fffffffffffffff;', 'helper(){value+=big 1;} init(){helper();if(value!=big 16r8000000000000000)value/=big 0;}', ''
    yield 'real-move', 'value:real; original:=3.5;', 'helper(){value=original;} init(){helper();if(value!=3.5){a:=17;a/=0;}}', ''
    yield 'caller-locals', 'value:=17;', 'helper(){value+=3;} init(){x:=big 17;y:=big 33;z:=1.25;helper();if(value!=20||x!=big 17||y!=big 33||z!=1.25)value/=0;}', ''
    yield 'scratch', 'a:=12345;b:=789;answer:int;', '\nhelper(){answer=((a^b)&65535)|65536;}\ninit(){x:=big 17;y:=big 33;helper();helper();if(answer!=78636||x!=big 17||y!=big 33)answer/=0;}', ''
    yield 'body-boundary', 'value:int;', 'helper(){'+('value++;'*32)+'} init(){helper();if(value!=32)value/=0;}', ''
    definitions = ' '.join('h'+str(i)+'(){value++;}' for i in range(15))
    calls = ''.join('h'+str(i)+'();' for i in range(15))
    yield 'function-boundary', 'value:int;', definitions+' init(){'+calls+'if(value!=15)value/=0;}', ''
    yield 'local-word', 'value:int;', 'helper(){x:int;x++;value+=x;} init(){helper();helper();if(value!=2)value/=0;}', ''
    yield 'local-loop', 'value:int;', 'helper(){x:int;x++;value+=x;} init(){for(i:=0;i<4097;i++)helper();if(value!=4097)value/=0;}', ''
    yield 'local-big', 'value:big;', 'helper(){x:big;x+=big 1;value+=x;} init(){helper();helper();if(value!=big 2)value/=big 0;}', ''
    yield 'local-real', 'value:real;', 'helper(){x:real;value=x;} init(){value=3.5;helper();if(value!=0.0){i:=1;i/=0;}}', ''
    yield 'local-mixed', 'value:int; wide:big; floating:real;', 'helper(){x:int;y:big;z:real;x++;y+=big 1;value+=x;wide+=y;floating=z;} init(){helper();helper();if(value!=2||wide!=big 2||floating!=0.0)value/=0;}', ''
    yield 'local-initialized', 'value:int;', 'helper(){x:=17;x++;value+=x;} init(){helper();helper();if(value!=36)value/=0;}', ''
    yield 'local-reuse', 'value:int; wide:big;', 'a(){x:big;x=big -1;wide=x;} b(){x:int;x++;value+=x;} init(){a();b();a();b();if(wide!=big -1||value!=2)value/=0;}', ''
    yield 'local-case-loop', 'value:int;', 'helper(){x:int;x++;value+=x;} init(){for(i:=0;i<4097;i++){case i%3 {0=>helper();1=>helper();2=>helper();* =>value/=0;}}if(value!=4097)value/=0;}', ''
    locals_ = ''.join('x'+str(i)+':big;' for i in range(12))
    updates = ''.join('x'+str(i)+'+=big 1;value+=x'+str(i)+';' for i in range(12))
    yield 'local-frame-boundary', 'value:big;', 'helper(){'+locals_+updates+'} init(){helper();helper();if(value!=big 24)value/=big 0;}', ''
    yield 'callee-error', 'value:=17;', '\nhelper(){value/=0;}\ninit(){helper();}', 'zero divide'
    yield 'wrong-answer', 'value:=17;', 'helper(){value+=3;} init(){helper();if(value!=21)value/=0;}', 'zero divide'

    yield 'helper-if', 'value:int;', 'helper(){if(value==0)value=7;else value+=9;} init(){helper();helper();if(value!=16)value/=0;}', ''
    yield 'helper-for', 'value:int;', 'helper(){x:int;for(i:=0;i<9;i++)x+=i;value+=x;} init(){helper();helper();if(value!=72)value/=0;}', ''
    yield 'helper-while', 'value:int;', 'helper(){x:int;while(x<9){x++;value+=x;}} init(){helper();helper();if(value!=90)value/=0;}', ''
    yield 'helper-zero-loop', 'value:int;', 'helper(){x:int;while(x<0)value++;} init(){helper();if(value!=0)value/=0;}', ''
    yield 'helper-nested', 'value:int;', 'helper(){x:int;for(i:=0;i<3;i++)for(j:=0;j<4;j++)x+=i+j;value+=x;} init(){helper();helper();if(value!=60)value/=0;}', ''
    yield 'helper-break', 'value:int;', 'helper(){x:int;for(i:=0;i<9;i++){if(i==3)continue;if(i==7)break;x+=i;}value+=x;} init(){helper();helper();if(value!=36)value/=0;}', ''
    yield 'helper-return', 'key:int;value:int;', 'helper(){if(key==0){value+=7;return;}value+=9;} init(){helper();key=1;helper();if(value!=16)value/=0;}', ''
    yield 'helper-big', 'value:big;', 'helper(){x:big;for(i:=big 0;i<big 9;i++)x+=i;value+=x;} init(){helper();helper();if(value!=big 72)value/=big 0;}', ''
    yield 'helper-real', 'value:int;key:=1.25;', 'helper(){if(key<2.0)value+=3;else value+=7;} init(){helper();key=3.5;helper();if(value!=10)value/=0;}', ''
    yield 'helper-after-entry', 'value:int;', 'init(){helper();helper();if(value!=12)value/=0;} helper(){for(i:=0;i<3;i++)value+=i+1;}', ''
    yield 'helper-multiple', 'value:int;', 'a(){if(value==0)value=1;else value+=2;} helper(){x:int;while(x<3){x++;value+=x;}} init(){a();helper();a();helper();if(value!=15)value/=0;}', ''
    yield 'helper-long-caller', 'value:int;', 'helper(){x:int;for(i:=0;i<4;i++)x+=i;value+=x;} init(){for(i:=0;i<4097;i++)helper();if(value!=24582)value/=0;}', ''
    yield 'helper-loop-error', 'value:int;', 'helper(){for(i:=0;i<4;i++){if(i==2)value/=0;value++;}} init(){helper();}', 'zero divide'
    yield 'helper-return-continue', 'value:int;', 'helper(){if(value==0)return;value++;} init(){helper();value/=0;}', 'zero divide'


# These sources must be valid ordinary Limbo before the scalar backend rejects their calling shapes.
def refusals():
    yield 'arguments', 'value:int;', 'helper(x:int){value+=x;} init(){helper(3);}'
    yield 'result', 'value:int;', 'helper():int{value++;return value;} init(){value=helper();}'
    yield 'recursive', 'value:int;', 'helper(){value++;helper();} init(){helper();}'
    yield 'nested', 'value:int;', 'inner(){value++;} helper(){inner();} init(){helper();}'
    yield 'byte-local', 'value:int;', 'helper(){x:byte;x=byte value;value=int x;} init(){helper();}'
    yield 'helper-table', 'value:=1;', 'helper(){case value {1=>value++;10 to 20=>value+=3;* =>value--;}} init(){helper();}'
    yield 'managed', 'value: string;', 'helper(){value="hello";} init(){helper();}'
    locals_ = ''.join('x'+str(i)+':big;' for i in range(13))
    updates = ''.join('x'+str(i)+'+=big 1;value+=x'+str(i)+';' for i in range(13))
    yield 'frame-limit', 'value:big;', 'helper(){'+locals_+updates+'} init(){helper();}'
    yield 'body-limit', 'value:int;', 'helper(){'+('value++;'*33)+'} init(){helper();}'
    body = ' '.join('h'+str(i)+'(){value++;}' for i in range(16))
    calls = ''.join('h'+str(i)+'();' for i in range(16))
    yield 'function-limit', 'value:int;', body+' init(){'+calls+'}'


# Derive the caller-local boundary from ordinary assembly, independently of the preparer's frame arithmetic.
def scratch_window(assembly, image, rows, locations, helperline):
    entrytype = int(re.search(r'\tentry\t\d+, (\d+)', assembly)[1])
    legacy = int(re.search(r'\tdesc\t\$'+str(entrytype)+r',(\d+),', assembly)[1])
    nsections = struct.unpack_from('>I', image, 28)[0]
    for i in range(nsections):
        kind, flags, at, size = struct.unpack_from('>HHII', image, 32+12*i)
        if kind == 3:
            at += 4
            data = struct.unpack_from('>I', image, at)[0]
            at += 8+(data+63)//64
            frame = struct.unpack_from('>I', image, at)[0]
            break
    else:
        raise RuntimeError('Missing frame descriptor')
    low, high = ((legacy+7)//8)*8+36, frame-16
    offsets = []
    for row, loc in zip(rows, locations):
        if not loc['start'] or loc['start']['line'] != helperline:
            continue
        mode, args, index = row['mode'], row['args'], 0
        for present, fp in ((bool(mode&0xc0),(mode&0xc0)==0x80),
                            (((mode>>3)&7)!=3,((mode>>3)&7)==1),
                            ((mode&7)!=3,(mode&7)==1)):
            if present:
                if fp:
                    offsets.append(args[index])
                index += 1
    if not offsets or any(value < low or value >= high for value in offsets):
        raise RuntimeError('Callee scratch overlaps caller storage')
    # A current-frame scratch address is valid VM storage but is not a callee-private cell.
    corrupted = offsets+[frame-12]
    if all(low <= value < high for value in corrupted):
        raise RuntimeError('Caller scratch control escaped')


# Replace the second call's zero stores with OR zero: valid scalar code that exposes stale local state.
def stale_locals(image, rows):
    result = bytearray(image)
    sections = struct.unpack_from('>I', image, 28)[0]
    code = next(struct.unpack_from('>HHII', image, 32+12*i)[2] for i in range(sections)
                if struct.unpack_from('>H', image, 32+12*i)[0] == 2)
    names = re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT/'include/isa.h').read_text(), re.M)
    stores, at = [], code+4
    for row in rows:
        if row['op'] == names.index('IMOVW') and row['mode'] == 0x11 and row['args'][0] == 0:
            stores.append(at)
        at += 2
        for unused in row['args']:
            first = image[at]
            at += 1 if first < 128 else 2 if first < 192 else 4
    if len(stores) < 8 or len(stores)%2:
        raise RuntimeError('Missing two complete helper initialization spans')
    for at in stores[len(stores)//2:]:
        result[at] = names.index('IORW')
    return result


# Compare real compiler products, preserve diagnostics, and execute whole OS processes with explicit target runners.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--emu', type=Path)
    parser.add_argument('--runner', default='')
    parser.add_argument('--reader', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    command = HELPERS['command']
    count = processes = controls = 0
    with tempfile.TemporaryDirectory(prefix='inline scalar ', dir=root) as directory:
        work = Path(directory)
        source, image, smap, assembly, legacy = (work/name for name in ('source.b','image.dis64','map.json','source.s','legacy.dis'))
        for name, globals_, body, error in programs():
            # A correct helper return must reach a distinct caller-only fault.
            if name.startswith('helper-') and not error:
                begin = body.index('{', body.index('init()'))
                depth = 1
                end = begin+1
                while depth:
                    depth += (body[end] == '{')-(body[end] == '}')
                    end += 1
                body = body[:end-1]+'continuation_probe <<=32;'+body[end-1:]
                globals_ += ' continuation_probe:int;'
                error = 'invalid argument'
            text = PREFIX+globals_+'\n'+body+'\n'
            source.write_text(text)
            command([args.limbo.resolve(), '-S', '-o', assembly, source])
            if '\tframe\t' not in assembly.read_text() or '\tcall\t' not in assembly.read_text():
                raise RuntimeError('Missing real source call: '+name)
            command([args.limbo.resolve(), '-o', legacy, source])
            before = legacy.read_bytes()
            command([sys.executable, ROOT/'utils/dis64-locations.py', 'compile', '--limbo', args.limbo.resolve(),
                     '--source', source, '--image', image, '--map', smap])
            rows = HELPERS['decoded'](image.read_bytes())
            locations = json.loads(smap.read_bytes())['instructions']
            if len(locations) != len(rows) or any(row['pc'] != pc or row['op'] != rows[pc]['op'] for pc,row in enumerate(locations)):
                raise RuntimeError('Prepared source-map identity mismatch')
            if name == 'local-frame-boundary' and not re.search(r'\tdesc\t\$\d+,128,', assembly.read_text()):
                raise RuntimeError('Missing actual 128-byte helper frame boundary')
            if name == 'scratch':
                helperline = next(i+1 for i,line in enumerate(text.splitlines()) if line.startswith('helper()'))
                scratch_window(assembly.read_text(), image.read_bytes(), rows, locations, helperline)
            if args.reader:
                command([args.reader.resolve(), image, '--pilot'])
            command([args.limbo.resolve(), '-o', legacy, source])
            if before != legacy.read_bytes():
                raise RuntimeError('Specialization changed ordinary Dis')
            if args.emu:
                for jit in (0,1):
                    output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                        '-c', str(jit), '-d', '/'+str(image.relative_to(root))])
                    marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="'+error+'"'
                    if marker not in output or ('Broken:' in output) != bool(error) or 'native64 scalar scheduler empty after program retirement' not in output or 'panic:' in output or 'faults:' in output:
                        raise RuntimeError(name+' JIT='+str(jit)+'\n'+output)
                    if name == 'callee-error':
                        faults = re.findall(r'native64 scalar fault: dis-pc=(\d+)',output)
                        line = next(i+1 for i,s in enumerate(text.splitlines()) if s.startswith('helper()'))
                        if len(faults)!=1 or locations[int(faults[0])]['start']['line']!=line:
                            raise RuntimeError('Inlined error lost helper source identity')
                    processes += 1
            if name == 'local-word':
                corrupted = work/'stale-locals.dis64'
                corrupted.write_bytes(stale_locals(image.read_bytes(), rows))
                if args.reader:
                    command([args.reader.resolve(), corrupted, '--pilot'])
                if args.emu:
                    for jit in (0,1):
                        output = HELPERS['EXECUTE'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root),
                            '-c', str(jit), '-d', '/'+str(corrupted.relative_to(root))])
                        marker = 'native64 scalar module InlineProbe: jit='+str(jit)+', exit="zero divide"'
                        if marker not in output or 'native64 scalar scheduler empty after program retirement' not in output:
                            raise RuntimeError('Stale helper locals control escaped\n'+output)
                        controls += 1
            count += 1
        for name,globals_,body in refusals():
            source.write_text(PREFIX+globals_+'\n'+body+'\n')
            command([args.limbo.resolve(), '-o', legacy, source])
            try:
                command([args.limbo.resolve(), '-6', '-o', image, source],False)
            except RuntimeError as failure:
                raise RuntimeError('Refusal shape accepted: '+name) from failure
            if image.exists():
                raise RuntimeError('Refused helper left an executable image: '+name)
    print('PASS scalar helpers:',count,'sources,',processes,'whole C/JIT processes;',controls,'active stale-local controls; ordinary Dis unchanged; 10 calling/storage limits refused')


if __name__ == '__main__':
    main()
