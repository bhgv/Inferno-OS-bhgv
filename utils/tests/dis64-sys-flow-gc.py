#!/usr/bin/env python3
"""Run String-rooted Word CFGs with the actual native binder, Sys handler and collector."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);args=parser.parse_args()
    old=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string-gc.py'))
    flow=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-flow.py'))
    arch=os.environ.get('NATIVE64_ARCH','amd64')
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))+['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DDIS_NATIVE64_MANAGED_FILES=1']
    if arch in ('arm64','riscv64'):command+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
    elif arch!='amd64':raise RuntimeError('unsupported architecture')
    command+=['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='sys-flow-gc-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);old['headers'](args.limbo,work)
        rows=flow['rows'](args.limbo,work);raw=flow['raw_cases'](args.limbo,work)
        output=['typedef struct SysFlowCase {const char *name; const uchar *image; unsigned int bytes; const char *events[16]; unsigned int calls; int fault;} SysFlowCase;']
        for i,(_,image,_,_) in enumerate(rows):output.append('static const uchar sysflow_image'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const SysFlowCase sysflow_cases[]={')
        for i,(name,_,events,status) in enumerate(rows):output.append('{"'+name+'",sysflow_image'+str(i)+',sizeof(sysflow_image'+str(i)+'),{'+(','.join('"'+v+'"' for v in events) if events else '0')+'},'+str(len(events))+','+str(int(bool(status)))+'},')
        output.append('};')
        for i,(_,image,_) in enumerate(raw):output.append('static const uchar sysflow_raw'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const struct {const uchar *image; unsigned int bytes; int accepted;} sysflow_raw[]={')
        for i,(_,_,accepted) in enumerate(raw):output.append('{sysflow_raw'+str(i)+',sizeof(sysflow_raw'+str(i)+'),'+str(int(accepted))+'},')
        output.append('};');(work/'sys-flow-images.h').write_text('\n'.join(output)+'\n')
        source=old['unit']().replace('int\nmain(void)','int\nsysstringbasemain(void)',1)+'\n'+(ROOT/'utils/tests/dis64-sys-flow-gc.c').read_text()
        (work/'test.c').write_text(source)
        common=command+['-I'+str(work),str(ROOT/'libinterp/dec.c'),str(ROOT/'lib9/rune.c'),'-Wl,--wrap=free','-lm']
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        subprocess.run(common+[str(work/'test.c'),'-o',str(work/'test')],check=True)
        subprocess.run(runner+[str(work/'test')],check=True,timeout=90)
        pilot=(ROOT/'include/dis64-pilot.h').read_text()
        marker='16U|((*target & state)&15U)'
        if pilot.count(marker)!=1:raise RuntimeError('intersection control marker drift')
        # Override the actual included proof; the malformed diamond must hit the admission predicate.
        override=work/'override';override.mkdir()
        (override/'dis64-pilot.h').write_text(pilot.replace(marker,'16U|((*target | state)&15U)'))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'control')],check=True)
        result=subprocess.run(runner+[str(work/'control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys flow admission predicate':raise RuntimeError('flow intersection control missed predicate: '+result.stdout+result.stderr)
        print('PASS active Sys String CFG intersection-removal control',flush=True)
        marker='memmove(ret, &result, sizeof(result));'
        if source.count(marker)!=1:raise RuntimeError('result control marker drift')
        (work/'word-control.c').write_text(source.replace(marker,'/* control: omit native Word publication */'))
        subprocess.run(common+[str(work/'word-control.c'),'-o',str(work/'word-control')],check=True)
        result=subprocess.run(runner+[str(work/'word-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=13 or result.stderr.strip()!='Sys flow Word-result predicate':raise RuntimeError('extended Word control missed predicate: '+result.stdout+result.stderr)
        print('PASS active extended Sys String Word-publication removal control',flush=True)


if __name__=='__main__':main()
