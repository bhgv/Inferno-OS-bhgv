#!/usr/bin/env python3
"""Real fixed Sys groups across generated links, release/GC callbacks and owner rollback."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]


def unit():
    old=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string-gc.py'))
    extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    source=old['unit']().replace('int\nmain(void)','int\nsysstringbasemain(void)',1)
    marker='static const LimboNative64Entry *const native64sysio[]={&limbo_native64_Sys_werrstr_entry};'
    source=source.replace(marker,'static const LimboNative64Entry *const native64sysio[]={&limbo_native64_Sys_werrstr_entry,&limbo_native64_Sys_chdir_entry,&limbo_native64_Sys_remove_entry};\nstatic void grouprelease(void);\nint kchdir(char*);int kremove(char*);')
    port=(ROOT/'emu/port/inferno.c').read_text();support='\n'.join(extract(port,signature) for signature in ('static void\nsysstatusframe(','void\nSys_chdir(','void\nSys_remove('))
    source=source.replace('void\nSys_werrstr(',support+'\nvoid\nSys_werrstr(',1)
    source=source.replace('void release(void) { require(cachereleases == cacheacquires); cachereleases++; }','void release(void) { require(cachereleases == cacheacquires); cachereleases++; grouprelease(); }')
    return source+'\n'+(ROOT/'utils/tests/dis64-sys-groups-gc.c').read_text()


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);args=parser.parse_args()
    old=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string-gc.py'));group=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-groups.py'))
    arch=os.environ.get('NATIVE64_ARCH','amd64')
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))+['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DDIS_NATIVE64_MANAGED_FILES=1']
    if arch in ('arm64','riscv64'):command+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
    elif arch!='amd64':raise RuntimeError('unsupported architecture')
    command+=['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='sys-groups-gc-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);old['headers'](args.limbo,work);rows=group['rows'](args.limbo,work)
        output=['typedef struct SysGroupEvent {const char *method,*argument; int status;} SysGroupEvent;','typedef struct SysGroupCase {const uchar *image;unsigned int bytes,imports;const char *names[3];unsigned int calls;SysGroupEvent events[8];} SysGroupCase;']
        for i,(_,image,_) in enumerate(rows):output.append('static const uchar group_image'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const SysGroupCase sysgroup_cases[]={')
        for i,(_,image,events) in enumerate(rows):
            names,_=group['imports'](image);output.append('{group_image'+str(i)+',sizeof(group_image'+str(i)+'),'+str(len(names))+',{'+','.join('"'+name+'"' for name in names)+'},'+str(len(events))+',{'+','.join('{"'+name+'","'+arg+'",'+str(status)+'}' for name,arg,status in events)+'}},')
        output.append('};');bad=list(group['refusals'](rows[0][1]))
        for i,image in enumerate(bad):output.append('static const uchar group_bad'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const struct {const uchar *image;unsigned int bytes;} sysgroup_bad[]={')
        for i in range(len(bad)):output.append('{group_bad'+str(i)+',sizeof(group_bad'+str(i)+')},')
        output.append('};');(work/'sys-group-images.h').write_text('\n'.join(output)+'\n')
        source=unit();(work/'test.c').write_text(source)
        common=command+['-I'+str(work),str(ROOT/'libinterp/dec.c'),str(ROOT/'lib9/rune.c'),'-Wl,--wrap=free','-lm']
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        subprocess.run(common+[str(work/'test.c'),'-o',str(work/'test')],check=True);subprocess.run(runner+[str(work/'test')],check=True,timeout=90)
        header=(ROOT/'include/dis64-sys-string.h').read_text();marker='if(seen & (1U<<kind)) return 0;'
        if header.count(marker)!=1:raise RuntimeError('duplicate control marker drift')
        override=work/'override';override.mkdir();(override/'dis64-sys-string.h').write_text(header.replace(marker,'/* control: accept duplicate methods */'))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'control')],check=True)
        result=subprocess.run(runner+[str(work/'control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys group admission predicate':raise RuntimeError('duplicate-import control missed predicate: '+result.stdout+result.stderr)
        print('PASS active fixed Sys duplicate-import removal control',flush=True)
        marker='memmove(f->ret, &result, sizeof(result));'
        if source.count(marker)!=2:raise RuntimeError('snapshot control marker drift: '+str(source.count(marker)))
        mutant=source.replace(marker,'{void *changed;memmove(&changed,(uchar*)fp+32,sizeof(changed));memmove(changed,&result,sizeof(result));}')
        (work/'snapshot-control.c').write_text(mutant);subprocess.run(common+[str(work/'snapshot-control.c'),'-o',str(work/'snapshot-control')],check=True)
        result=subprocess.run(runner+[str(work/'snapshot-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=17 or result.stderr.strip()!='Sys group result predicate':raise RuntimeError('snapshot control missed predicate: '+result.stdout+result.stderr)
        print('PASS active fixed Sys captured-result removal control',flush=True)


if __name__=='__main__':main()
