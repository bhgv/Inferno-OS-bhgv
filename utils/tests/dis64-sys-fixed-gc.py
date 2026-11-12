#!/usr/bin/env python3
"""Actual mixed fixed Sys handlers through production module construction, native frames and GC."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]


def unit():
    old=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-groups-gc.py'))
    extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    source=old['unit']().replace('int\nmain(void)','int\ngroupbasemain(void)',1)
    marker='&limbo_native64_Sys_remove_entry};'
    source=source.replace(marker,'&limbo_native64_Sys_remove_entry,&limbo_native64_Sys_millisec_entry,&limbo_native64_Sys_sleep_entry,&limbo_native64_Sys_dup_entry};',1)
    source=source.replace('module->type=mallocz(3*sizeof(Type*),1);module->ext=mallocz(4*sizeof(Link),1);','module->type=mallocz(6*sizeof(Type*),1);module->ext=mallocz(7*sizeof(Link),1);').replace('for(i=0;i<3;i++){\n        entry=native64sysio[i];','for(i=0;i<6;i++){\n        entry=native64sysio[i];',1)
    source=source.replace('static void\ngrouprelease(void)\n{','static void\ngroupstringrelease(void)\n{',1)
    port=(ROOT/'emu/port/inferno.c').read_text()
    support='char Ebadarg[]="bad argument";\n'+'\n'.join(extract(port,signature) for signature in ('static int\nsysnativeint(','void\nSys_millisec(','void\nSys_dup(','void\nSys_sleep('))
    source=source.replace('void\nSys_werrstr(',support+'\nvoid\nSys_werrstr(',1)
    return source+'\n'+(ROOT/'utils/tests/dis64-sys-fixed-gc.c').read_text()


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);args=parser.parse_args()
    old=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-groups-gc.py'));support=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string-gc.py'));fixed=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-fixed.py'))
    arch=os.environ.get('NATIVE64_ARCH','amd64')
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))+['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DDIS_NATIVE64_MANAGED_FILES=1']
    if arch in ('arm64','riscv64'):command+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
    elif arch!='amd64':raise RuntimeError('unsupported architecture')
    command+=['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='sys-fixed-gc-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);support['headers'](args.limbo,work)
        # Retain the old group fixture declarations for the unused regression helpers.
        output=['typedef struct SysGroupEvent {const char *method,*argument; int status;} SysGroupEvent;','typedef struct SysGroupCase {const uchar *image;unsigned int bytes,imports;const char *names[3];unsigned int calls;SysGroupEvent events[8];} SysGroupCase;','static const SysGroupCase sysgroup_cases[1]={{0}};','static const struct {const uchar *image;unsigned int bytes;} sysgroup_bad[1]={{0}};']
        (work/'sys-group-images.h').write_text('\n'.join(output)+'\n')
        rows=fixed['rows'](args.limbo,work);output=['typedef struct SysFixedEvent {const char *method,*text;WORD first,second,status;} SysFixedEvent;','typedef struct SysFixedCase {const uchar *image;unsigned int bytes,imports;const char *names[6];unsigned int calls;SysFixedEvent events[16];} SysFixedCase;']
        for i,(_,image,_) in enumerate(rows):output.append('static const uchar fixed_image'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const SysFixedCase sysfixed_cases[]={')
        for i,(_,image,events) in enumerate(rows):
            names,_,_=fixed['imports'](image)
            def event(e):
                name,text,first,second,status=e
                return '{"'+name+'",'+('"'+text+'"' if text is not None else '0')+','+str(first)+','+str(second)+','+str(status)+'}'
            output.append('{fixed_image'+str(i)+',sizeof(fixed_image'+str(i)+'),'+str(len(names))+',{'+','.join('"'+name+'"' for name in names)+'},'+str(len(events))+',{'+','.join(event(e) for e in events)+'}},')
        output.append('};');bad=list(fixed['refusals'](args.limbo,work))
        for i,image in enumerate(bad):output.append('static const uchar fixed_bad'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const struct {const uchar *image;unsigned int bytes;} sysfixed_bad[]={')
        for i in range(len(bad)):output.append('{fixed_bad'+str(i)+',sizeof(fixed_bad'+str(i)+')},')
        output.append('};');(work/'sys-fixed-images.h').write_text('\n'.join(output)+'\n')
        source=unit();(work/'test.c').write_text(source)
        common=command+['-I'+str(work),str(ROOT/'libinterp/dec.c'),str(ROOT/'lib9/rune.c'),'-Wl,--wrap=free','-lm']
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        subprocess.run(common+[str(work/'test.c'),'-o',str(work/'test')],check=True);subprocess.run(runner+[str(work/'test')],check=True,timeout=90)
        header=(ROOT/'include/dis64-sys-string.h').read_text();marker='if(++parameter == dis64sysfixedargs(kinds[ordinal])) phase = 4;'
        if header.count(marker)!=1:raise RuntimeError('argument-fact control marker drift')
        override=work/'override';override.mkdir();(override/'dis64-sys-string.h').write_text(header.replace(marker,'flow[i]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP),0,0,72};\n'+marker))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'control')],check=True)
        result=subprocess.run(runner+[str(work/'control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys fixed admission predicate':raise RuntimeError('argument-fact control missed predicate: '+result.stdout+result.stderr)
        print('PASS active fixed Sys argument-store fact pollution control',flush=True)


if __name__=='__main__':main()
