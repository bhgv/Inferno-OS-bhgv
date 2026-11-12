#!/usr/bin/env python3
"""Real Sys_werrstr with serialized modules, production GC and fault injection."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]


def unit():
    gc=runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-gc.py'))
    extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    source=gc['unit'](managed=True).replace('int\nmain(void)','int\nmanagedbasemain(void)',1)
    support='''
#define DIS_NATIVE64_BOOT_SYS_IO 1
#include "dis64-sys-string.h"
#include "native64-model.h"
#define LIMBO_NATIVE64_TABLES
#include "sys-native64.h"
#undef LIMBO_NATIVE64_TABLES
static const LimboNative64Entry *const native64sysio[]={&limbo_native64_Sys_werrstr_entry};
ulong msize(void *pointer)
{
    unsigned int i;
    for(i=0;i<RuntimeBlocks;i++)if(blocks[i].pointer == pointer)return blocks[i].bytes;
    require(0);return 0;
}
'''
    source=source.replace(extract(source,'char*\nstring2c('),'',1)
    strings=(ROOT/'libinterp/string.c').read_text()
    support+='#define malloc(n) trackedallocate(n,0)\n'+extract(strings,'char*\nstring2c(')+'\n#undef malloc\n'
    support+=extract((ROOT/'emu/port/chan.c').read_text(),'void\nkstrcpy(')+'\n'
    support+=extract((ROOT/'emu/port/inferno.c').read_text(),'void\nSys_werrstr(')+'\n'
    support+=extract((ROOT/'libinterp/runt.c').read_text(),'const Link*\nnative64sysexport(')+'\n'
    marker='#ifdef DIS_NATIVE64_MANAGED_FILES\n/* Generated'
    before,after=source.rsplit(marker,1)
    source=before+support+marker+after
    return source+'\n'+(ROOT/'utils/tests/dis64-sys-string-gc.c').read_text()


def headers(limbo,work):
    gc=runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-gc.py'))
    fixtures=runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'))
    (work/'managed-images.h').write_text(gc['managed_images'](fixtures,limbo,work))
    groups=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-groups.py'));(work/'group-images.h').write_text(groups['header'](fixtures,limbo,work))
    literals=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-literal.py'));(work/'literal-images.h').write_text(literals['header'](limbo,work))
    named=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-module.py'));(work/'named-seeds.h').write_text(named['seed_header'](limbo,work))
    for module in ('string','sys'):
        text=subprocess.check_output([str(limbo.resolve()),'-H','-M',module.title(),'-I'+str(ROOT/'module'),str(ROOT/('module/'+module+'.m'))],text=True)
        (work/(module+'-native64.h')).write_text(text)
        if module=='string':(work/'string-hashes.h').write_text('\n'.join(line for line in text.splitlines() if line.startswith(('#define F_', '#define LIMBO_NATIVE64_SIGNATURE_', '#define LIMBO_NATIVE64_ARGUMENT_', '#define LIMBO_NATIVE64_RESULT_')))+'\n')
    program=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string.py'))
    rows=program['rows'](limbo,work)
    output=['typedef struct SysStringCase {const uchar *image; unsigned int bytes; const char *expected; unsigned int calls, same;} SysStringCase;']
    for i,(image,value) in enumerate(rows):output.append('static const uchar sysstring_image'+str(i)+'[]={'+','.join(map(str,image))+'};')
    output.append('static const SysStringCase sysstring_cases[]={')
    for i,(image,value) in enumerate(rows):
        data=value.split('\0')[0].encode();text='"'+''.join('\\%03o'%v for v in data)+'"'
        calls=2 if i<12 and i%2 else 32 if i==13 else 1
        output.append('{sysstring_image'+str(i)+',sizeof(sysstring_image'+str(i)+'),'+text+','+str(calls)+','+str(int(i<12))+'},')
    output.append('};')
    bad=list(program['refusals'](rows[0][0]))
    for i,image in enumerate(bad):output.append('static const uchar sysstring_bad'+str(i)+'[]={'+','.join(map(str,image))+'};')
    output.append('static const struct {const uchar *image; unsigned int bytes;} sysstring_bad[]={')
    for i in range(len(bad)):output.append('{sysstring_bad'+str(i)+',sizeof(sysstring_bad'+str(i)+')},')
    output.append('};');(work/'sys-string-images.h').write_text('\n'.join(output)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);args=parser.parse_args()
    arch=os.environ.get('NATIVE64_ARCH','amd64')
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))+['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DDIS_NATIVE64_MANAGED_FILES=1']
    if arch in ('arm64','riscv64'):command+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
    elif arch!='amd64':raise RuntimeError('unsupported architecture')
    command+=['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='sys-string-gc-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);headers(args.limbo,work);source=unit();(work/'test.c').write_text(source)
        common=command+['-I'+str(work),str(ROOT/'libinterp/dec.c'),str(ROOT/'lib9/rune.c'),'-Wl,--wrap=free','-lm']
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        subprocess.run(common+[str(work/'test.c'),'-o',str(work/'test')],check=True)
        subprocess.run(runner+[str(work/'test')],check=True,timeout=90)
        # The real handler's result must reach its bound Word without corrupting padding.
        marker='memmove(ret, &result, sizeof(result));'
        if source.count(marker)!=1:raise RuntimeError('Word-result control marker drift')
        mutant=source.replace(marker,'/* control: omit result publication */',1)
        (work/'control.c').write_text(mutant)
        subprocess.run(common+[str(work/'control.c'),'-o',str(work/'control')],check=True)
        result=subprocess.run(runner+[str(work/'control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=13 or result.stderr.strip()!='Sys String Word-result predicate':raise RuntimeError('Word result control missed predicate: '+result.stdout+result.stderr)
        print('PASS active Sys String result-publication removal control',flush=True)
        marker='memmove(file->caller->MP+24+8*j,&literal,sizeof(literal));'
        if source.count(marker) not in (1,2):raise RuntimeError('Sys literal publication control marker drift')
        (work/'root-control.c').write_text(source.replace(marker,'/* control: omit MP literal publication */').replace('sysstringrollback(sysstring_cases+i,&native,&heap);','/* control: reach the explicit execution value predicate */'))
        subprocess.run(common+[str(work/'root-control.c'),'-o',str(work/'root-control')],check=True)
        result=subprocess.run(runner+[str(work/'root-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=12 or not result.stderr.startswith('Sys String error-text predicate:'):raise RuntimeError('Sys root control missed predicate: '+result.stdout+result.stderr)
        print('PASS active Sys String literal-publication removal control',flush=True)



if __name__=='__main__':main()
