#!/usr/bin/env python3
"""Relocate real startup MP initializers into owned native data images; never execute AWM."""
import argparse
import json
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
AWM = runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))
READ = runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))
LAYOUT = runpy.run_path(str(ROOT/'utils/tests/dis64-layout.py'))


def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


def relocate(plan, initial):
    """Trusted compiler geometry plus independently decoded classic constants."""
    records, strings = [], []
    require(plan['accepted'] and plan['space'] == 'mp', 'unproved MP layout')
    for offset, value in sorted(initial.items()):
        width = {'byte': 1, 'word': 4, 'long': 8, 'real': 8, 'string': 4}[value['kind']]
        leaves = [l for l in plan['leaves'] if l[0] <= offset and offset+width <= l[0]+l[2]]
        require(len(leaves) == 1, 'initializer crosses typed storage')
        old, native, oldbytes, size, kind = leaves[0]
        root = oldbytes == 4 and size == 8 and kind >= 0
        require(root == (value['kind'] == 'string'), 'initializer root role mismatch')
        target = native+offset-old
        if root:
            require(offset == old and target % 8 == 0, 'partial string root')
            # Validate the complete UTF-8 before the native String constructor.
            bytes(value['bytes']).decode('utf-8')
            strings.append((target, value['bytes']))
        else:
            require(size == oldbytes and target % width == 0, 'scalar relocation width/alignment')
            records.append([target, width, (value['high'] << 32) | value['low']])
    records.sort()
    require(all(a[0]+a[1] <= b[0] for a, b in zip(records, records[1:])), 'overlapping initializers')
    return records, strings


def generate(limbo, work, selected=None, control=None):
    require(__debug__, 'independent compiler/legacy oracles require Python assertions')
    lines = ['#include "native64-typed-module.h"',
             'static const Native64TypedStep emptycode[]={{IRET,SRC(AXXX)|DST(AXXX),0,0,0,0}};',
             'static const uchar emptymap[]={0};',
             'static const Native64ModuleFunction emptyfn[]={ {"data_only",0,0,emptycode,1,64,emptymap,1,nil,0} };']
    cases, blocked, counts = [], [], {'strings': 0, 'scalars': 0, 'opaque': 0}
    for ordinal, source in enumerate(AWM['STARTUP']):
        if selected and source not in selected:
            continue
        image = work/(str(ordinal)+'.dis')
        command = [str(limbo), '-I', 'module', '-o', str(image), source]
        READ['checked_run'](command, cwd=ROOT, check=True, capture_output=True)
        original = image.read_bytes()
        result = READ['checked_run'](command, cwd=ROOT, check=True, capture_output=True,
                                    env=dict(os.environ, LIMBO_DIS64_LAYOUT='1'))
        require(image.read_bytes() == original, 'layout diagnostics changed classic bytes')
        plan = next(json.loads(l[13:]) for l in result.stderr.decode().splitlines()
                    if l.startswith('DIS64_LAYOUT ') and json.loads(l[13:])['space'] == 'mp')
        _, _, types = READ['legacy_instructions'](original, with_types=True)
        require(LAYOUT['verify'](plan, types) == 'accepted', 'independent layout proof failed')
        try:
            _, _, _, initial = READ['legacy_instructions'](original, with_types=True, with_data=True)
        except RuntimeError as error:
            if str(error) != 'Non-flat classic initializer in this bounded source gate':
                raise
            blocked.append(source)
            continue
        records, strings = relocate(plan, initial)
        name = 'mp'+str(ordinal)
        maps = list(plan['map'])
        if control == 'scalar' and not cases:
            require(records, 'scalar control needs an initialized scalar')
            records[0][2] ^= 1
        if control == 'map' and not cases:
            require(strings, 'map control needs an initialized String')
            bit = strings[0][0]//8
            maps[bit//8] ^= 0x80 >> (bit % 8)
        lines += [f'static const uchar {name}map[]={{'+','.join(map(str, maps))+'};',
                  f'static const Native64ModuleInit {name}init[]={{']
        lines += ['{'+f'{at},{width},{bits}ULL'+'},' for at, width, bits in records] or ['{0},']
        lines += ['};', f'static const MPLeaf {name}leaves[]={{']
        lines += ['{'+','.join(map(str, leaf))+'},' for leaf in plan['leaves']]
        lines += ['};']
        # Expected legacy cells and target leaves are separate C inputs; the C
        # oracle resolves offsets itself, without using relocated record offsets.
        for i, (offset, value) in enumerate(sorted(initial.items())):
            if value['kind'] == 'string':
                raw = value['bytes']
                runes = list(bytes(raw).decode('utf-8').encode('utf-32-le'))
                points = [int.from_bytes(bytes(runes[j:j+4]), 'little') for j in range(0, len(runes), 4)]
                lines += [f'static const uchar {name}s{i}[]={{'+','.join(map(str, raw or [0]))+'};',
                          f'static const Rune {name}r{i}[]={{'+','.join(map(str, points or [0]))+'};']
        lines += [f'static const MPCell {name}cells[]={{']
        for i, (offset, value) in enumerate(sorted(initial.items())):
            if value['kind'] == 'string':
                raw = value['bytes']
                lines += ['{'+f'{offset},0,0,{name}s{i},{len(raw)},{name}r{i},{len(bytes(raw).decode("utf-8"))}'+'},']
                counts['strings'] += 1
            else:
                width = {'byte': 1, 'word': 4, 'long': 8, 'real': 8}[value['kind']]
                bits = (value['high'] << 32) | value['low']
                lines += ['{'+f'{offset},{width},{bits}ULL,nil,0,nil,0'+'},']
                leaf = next(l for l in plan['leaves'] if l[0] <= offset < l[0]+l[2])
                counts['opaque' if leaf[4] < 0 else 'scalars'] += 1
        lines += ['};', f'static const Native64ModuleSpec {name}spec={{'+
                  f'"AWMData{ordinal}","{source}",emptyfn,1,~0U,{plan["bytes"]},{name}map,{len(maps)},{name}init,{len(records)},nil,0'+'};']
        cases.append('{'+f'&{name}spec,{name}leaves,{len(plan["leaves"])},{name}cells,{len(initial)}'+'}')
    require(cases, 'no flat startup data images selected')
    if not selected:
        require(set(blocked) == {'appl/wm/atoolbar.b', 'appl/lib/titlebar.b', 'appl/cmd/sh/sh.b'},
                'nested initializer frontier changed; review required')
    lines += ['static const MPCase mpcases[]={'+','.join(cases)+'};']
    (work/'awm-data.h').write_text('\n'.join(lines)+'\n')
    return len(cases), blocked, counts


def unit():
    source = runpy.run_path(str(ROOT/'utils/tests/native64-typed-module.py'))['unit']()
    source = source.replace('require(int condition)', 'baserequire(int condition,const char *expression,int line)', 1)
    source = source.replace('checks);\n\t\tabort();', 'checks);fprintf(stderr,"AWM fixture predicate line=%d: %s\\n",line,expression);exit(10);', 1)
    source = source.replace('/* Bound each allocation', '#define require(c) baserequire(!!(c),#c,__LINE__)\n/* Bound each allocation', 1)
    extract = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    for signature in ('String*\nnewstring(', 'String*\nnewrunes('):
        source = source.replace(extract(source, signature), '', 1)
    source += '\nchar exNegsize[]="negative size";\n'
    source += 'int hmsize(void*h){for(unsigned i=0;i<RuntimeBlocks;i++)if(blocks[i].pointer==(uchar*)h-O(Bhdr,u.data))return blocks[i].bytes-O(Bhdr,u.data);require(0);return 0;}\n'
    source += '\n#undef T\n'+'\n'.join(l for l in (ROOT/'lib9/rune.c').read_text().splitlines() if not l.startswith('#include'))+'\n#undef T\n'
    strings = (ROOT/'libinterp/string.c').read_text()
    source += '\n'+'\n'.join(extract(strings, s) for s in ('String*\nnewstring(', 'String*\nnewrunes(', 'String*\nc2string('))
    source += '\n'+extract((ROOT/'libinterp/heap.c').read_text(), 'void\nfreestring(')+'\n'
    return source+'\n'+(ROOT/'utils/tests/awm-native64-data.c').read_text()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--source', action='append', choices=AWM['STARTUP'])
    parser.add_argument('--control', choices=('scalar', 'map', 'clone'))
    args = parser.parse_args()
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    with tempfile.TemporaryDirectory(prefix='awm-data-', dir=ROOT) as td:
        work = Path(td)
        count, blocked, totals = generate(args.limbo.resolve(), work, args.source, args.control)
        source = work/'test.c'; text = unit()
        if args.control == 'clone':
            extract = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
            body = extract(text, 'void\nnewmp(')
            require(body.count('h->ref++;') == 1, 'clone retain marker drift')
            text = text.replace(body, body.replace('h->ref++;', ';'), 1)
        source.write_text(text)
        cc = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        if arch in ('arm64', 'riscv64'):
            cc += ['-DDIS64_TEST_'+arch.upper()+'=1', '-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch != 'amd64':
            parser.error('unsupported target '+arch)
        binary = work/'test'
        cc += ['-DDIS_WORD32=1']+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        subprocess.run(cc+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER', ''))+[str(binary)], capture_output=True, text=True, timeout=180)
        if args.control:
            marker = {'scalar': 'scalar-value', 'map': 'string-map', 'clone': 'string-refcount'}[args.control]
            require(result.returncode == 10 and 'AWM data predicate: '+marker in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr, result.stderr)
            print('PASS AWM MP active control:', arch, args.control)
        else:
            require(result.returncode == 0, result.stderr)
            print(result.stdout, end='')
            print('PASS AWM data relocation:', arch, count, 'images;', totals, '; nested initializers still refused:', ', '.join(blocked))
            print('Data construction/cloning only; legacy code coordinates retained; no AWM instruction executed.')


if __name__ == '__main__':
    main()
