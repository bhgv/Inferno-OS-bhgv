#!/usr/bin/env python3
"""Check typed migration layouts against native C and actual AWM legacy Dis descriptors."""
import argparse
from collections import Counter
import importlib.util
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT/'utils/tests'/filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(command, **kwargs):
    result = subprocess.run(list(map(str, command)), stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=120, **kwargs)
    if result.returncode or any(mark in result.stderr for mark in
                                (b'AddressSanitizer', b'runtime error:', b'UndefinedBehaviorSanitizer')):
        raise RuntimeError(shlex.join(list(map(str, command)))+'\n'+result.stderr.decode(errors='replace'))
    return result


def verify(record, descriptors):
    if not record['accepted']:
        return record['reason']
    oldsize, oldmap = descriptors[record['descriptor']]
    assert oldsize == record['legacy_bytes']
    assert 0 <= record['bytes'] <= 1024*1024 and record['bytes'] % 8 == 0
    assert len(record['map']) == (record['bytes']+63)//64
    roots, expected, occupied = set(), bytearray(len(record['map'])), set()
    previous = 0
    for old, new, oldbytes, size, kind in record['leaves']:
        assert old >= previous and 0 < oldbytes <= oldsize-old
        assert new >= (64 if record['space'] == 'fp' else 0) and 0 < size <= record['bytes']-new
        previous = old+oldbytes
        assert not occupied.intersection(range(new, new+size))
        occupied.update(range(new, new+size))
        root = oldbytes == 4 and size == 8 and kind >= 0
        if root:
            assert old % 4 == new % 8 == 0
            roots.add(old//4)
            expected[new//64] |= 0x80 >> (new//8 % 8)
        elif oldbytes == 4 and size == 8:
            assert kind == -1 and new % 8 == 0  # Borrowed addresses are never GC roots.
    actual = {i*8+bit for i, byte in enumerate(oldmap) for bit in range(8) if byte & (0x80 >> bit)}
    assert actual == roots and list(expected) == record['map']
    return 'accepted'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--c-only', action='store_true')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    env.pop('LIMBO_DIS64_LAYOUT', None)
    awm = load('awm_layout', 'awm-native64-preflight.py')
    reader = load('legacy_layout_reader', 'native64-callsites.py')
    with tempfile.TemporaryDirectory(prefix='typed-layout-', dir=ROOT) as temp:
        work = Path(temp)
        binary = work/'layout'
        native = (ROOT/'limbo/native64.c').read_text()
        start = native.index('static int\nnative64sourcelayout(')
        opening = native.index('{', start)
        end, depth = opening+1, 1
        while depth:
            depth += (native[end]=='{')-(native[end]=='}')
            end += 1
        structure = native[native.index('typedef struct Native64SourceLayout'):start]
        wrapper = work/'bank.c'
        wrapper.write_text('#include "limbo.h"\n#include "dis64-layout.h"\n#include <assert.h>\n'+structure+native[start:end]+r'''
int layoutbanktest(void)
{
    Native64SourceLayout bank={0};
    Type good={0},bad={0}; Decl a={0},b={0}; Desc desc={0}; uchar map[]={0x80};
    good.kind=Tstring; good.size=4; bad.kind=Tfn; bad.size=4;
    a.store=b.store=Dglobal; a.ty=&good; b.ty=&bad; a.next=&b; b.offset=4;
    desc.size=8; desc.nmap=1; desc.map=map;
    assert(!native64sourcelayout(&desc,&a,nil,0,&bank));
    assert(bank.typed.leaves==nil && bank.typed.reason==nil && bank.typed.count==0);
    assert(bank.offsets==nil && bank.map==nil && bank.bytes==0);
    return 1;
}
''')
        cc = shlex.split(env.get('CC', 'cc'))
        flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        build = cc+flags+['-Dfree=layouttestfree', '-ffunction-sections', '-fdata-sections', '-I'+str(ROOT/'Linux/386/include'),
                     '-I'+str(ROOT/'include'), '-I'+str(ROOT/'limbo'),
                     ROOT/'limbo/dis64-layout.c', ROOT/'utils/tests/dis64-layout.c', wrapper,
                     '-Wl,--gc-sections', '-o', binary]
        runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
        run(build, cwd=ROOT, env=env)
        run(runner+[binary], cwd=ROOT, env=env)
        print('PASS typed layouts: native C geometry, nested fields, aliases, picks, alt address roles, cases and malformed metadata', flush=True)
        production = (ROOT/'limbo/dis64-layout.c').read_text()
        controls = (
            ('borrowed-root', 'l->kind>=0 && l->oldbytes==4', 'l->oldbytes==4',
             '!dis64layoutaddress(&p,12,4,1,&at)'),
            ('descriptor-role', 'if(map[i]!=(i<desc->nmap?desc->map[i]:0))', 'if(0)',
             '!dis64layoutstorage(args,&local,&desc,1,&p)'),
            ('code-as-scalar', 'l->kind<0 || l->bytes!=l->oldbytes', 'l->bytes!=l->oldbytes',
             '!dis64layoutaddress(&p,i==2?24:12,4,0,&at)'),
        )
        for label, before, after, predicate in controls:
            assert production.count(before) == 1
            altered = work/(label+'.c')
            altered.write_text(production.replace(before, after))
            command = [altered if str(x) == str(ROOT/'limbo/dis64-layout.c') else x for x in build]
            run(command, cwd=ROOT, env=env)
            result = subprocess.run(list(map(str, runner+[binary])), cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            assert result.returncode and b'Assertion' in result.stderr and predicate.encode() in result.stderr, result.stderr
            assert b'runtime error:' not in result.stderr and b'AddressSanitizer' not in result.stderr, result.stderr
            print('PASS active typed-layout '+label+' predicate control', flush=True)
        if args.c_only:
            return
        counts = Counter()
        for ordinal, source in enumerate(awm.STARTUP):
            original, planned = work/f'{ordinal}.ordinary.dis', work/f'{ordinal}.planned.dis'
            command = ['-I', 'module', '-o']
            baseline = (args.baseline or args.limbo).resolve()
            run([baseline, *command, original, source], cwd=ROOT, env=env)
            result = run([args.limbo.resolve(), *command, planned, source], cwd=ROOT,
                         env=dict(env, LIMBO_DIS64_LAYOUT='1'))
            assert original.read_bytes() == planned.read_bytes(), source
            header, code, descriptors = reader.legacy_instructions(planned.read_bytes(), with_types=True)
            records = [json.loads(line[13:]) for line in result.stderr.decode().splitlines()
                       if line.startswith('DIS64_LAYOUT ')]
            assert records and sum(r['space'] == 'mp' for r in records) == 1
            assert len({(r['space'], r['pc']) for r in records}) == len(records)
            for record in records:
                assert record['schema'] == 'inferno-layout-v1'
                if record['space'] == 'fp':
                    assert 0 <= record['pc'] < len(code)
                counts[verify(record, descriptors)] += 1
            target = work/f'{ordinal}.managed.dis64'
            refused = subprocess.run([str(args.limbo.resolve()), '-64l', '-I', 'module',
                                      '-o', str(target), source], cwd=ROOT,
                                     env=dict(env, LIMBO_DIS64_LAYOUT='1'),
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
            assert refused.returncode > 0 and not target.exists() and b'Dis64 managed trial:' in refused.stderr
            managed = [json.loads(line[13:]) for line in refused.stderr.decode().splitlines()
                       if line.startswith('DIS64_LAYOUT ')]
            assert managed == records, source
            print(f'PASS {source}: {sum(r["accepted"] for r in records)}/{len(records)} typed banks; byte-identical legacy output', flush=True)
        assert counts['accepted'] > 100, counts
        assert set(counts) == {'accepted'}, counts
        print(f'PASS all {len(awm.STARTUP)} AWM startup sources: {counts["accepted"]} typed storage plans; compiler evidence only, no managed execution')


if __name__ == '__main__':
    main()
