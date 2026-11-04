#!/usr/bin/env python3
"""Build owned numeric Modules from raw mapped storage and run actual GC/rollback bodies."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
RAW=runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-storage.py'))
W,I,D,IMAGE,SECTIONS=RAW['W'],RAW['I'],RAW['D'],RAW['IMAGE'],RAW['SECTIONS']
EXIT=RAW['EXIT']


# Code bytes/expected results are independent of the C reader/writer or any declaration sidecar.
def cases():
    rows=[]
    for ordinal in (0,1,64):
        types=[D(80,8)]*(ordinal+1) if ordinal else [D(16,8,b'\x80'),D(80,8)]
        if ordinal: types[ordinal]=D(16,8,b'\x80')
        frame=1 if ordinal==0 else 0
        raw=IMAGE(SECTIONS(datatype=ordinal,frame=frame,types=types,data=W(1,8,1,40),code=W(2)+I('IADDW',16,source=2,destination=8)+EXIT))
        rows.append(('numeric-'+str(ordinal),1,raw,42,8,4,0))
    base=dict(data=W(1,8,1,40),code=W(2)+I('IADDW',16,source=2,destination=8)+EXIT)
    rows += [('mapped-low',0,IMAGE(SECTIONS(data=W(0),code=W(2)+I('IMOVW',16,source=42,destination=0)+EXIT)),0,8,4,0),
             ('mapped-high',0,IMAGE(SECTIONS(data=W(0),code=W(2)+I('IMOVW',16,source=42,destination=4)+EXIT)),0,8,4,0),
             ('mapped-read',0,IMAGE(SECTIONS(data=W(0),code=W(2)+I('IMOVW',0,source=0,destination=8)+EXIT)),0,8,4,0),
             ('uninitialized-fp',0,IMAGE(SECTIONS(data=W(0),code=W(2)+I('IMOVW',8,source=64,destination=8)+EXIT)),0,8,4,0),
             ('frame-map',0,IMAGE(SECTIONS(**base,types=[D(80,8,b'\x80\x00'),D(16,8,b'\x80')])),0,8,4,0),
             ('import',0,IMAGE(SECTIONS(**base,imports=[RAW['RAW']['binding']()])),0,8,4,0),
             ('missing-data',0,IMAGE(SECTIONS(code=base['code'])),0,8,4,0),
             ('long-middle',0,IMAGE(SECTIONS(data=W(0),code=W(2)+I('IADDL',80,source=2,middle=1,destination=8)+EXIT)),0,8,4,0)]
    rows += [('long-immediate',1,IMAGE(SECTIONS(data=W(0),code=W(2)+I('IMOVL',16,source=-1,destination=8)+EXIT)),(1<<64)-1,8,8,0),
             ('float-copy',1,IMAGE(SECTIONS(databytes=32,types=[D(80,8),D(32,8,b'\x80')],data=W(1,8,3,0x40454000,0),code=W(2)+I('IMOVF',0,source=8,destination=16)+EXIT)),0x4045400000000000,16,8,0),
             ('long-shift',1,IMAGE(SECTIONS(databytes=32,types=[D(80,8),D(32,8,b'\x80')],data=W(2,12,1,1,16,2,0,21),code=W(2)+I('ISHLL',0,source=12,destination=16)+EXIT)),42,16,8,0),
             ('division-zero',1,IMAGE(SECTIONS(data=W(1,8,1,40),code=W(2)+I('IDIVW',16,source=0,destination=8)+EXIT)),40,8,4,1)]
    rows.append(('empty-exit',1,IMAGE(SECTIONS(databytes=0,types=[D(64,8)],code=W(1)+EXIT)),0,0,0,0))
    return rows


# Copy only the new numeric block and native lowering helpers into the actual collector/owner closure.
def numericparts():
    fn=runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    load=(ROOT/'libinterp/load64.c').read_text()
    body='\n'.join(fn(load,s) for s in ('static int\ndis64longsource(', 'static int\ndis64runtimeinstruction(', 'static void\ndis64runtimeinst('))
    body+='\n'+load[load.index('typedef struct Dis64NumericLoad'):]
    return body


# Exact-stage omissions must fail semantically before publication/unsafe retirement.
def unit(control):
    fn=runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    source=runpy.run_path(str(ROOT/'utils/tests/dis64-storage-attach.py'))['unit'](None)
    source=source.replace('int\nmain(int argc, char **argv)','int\nstorage_attach_main(int argc, char **argv)',1)
    source+='\n#include "dis64-numeric-storage.h"\n'
    source+='#define Dis64RuntimeName 255U // Actual numeric display-name policy.\n'
    parts=numericparts()
    changes={'entry':('module->entry = module->prog+load->image.module.entry;','module->entry = nil;'),
             'path':('module->path = strdup(load->path);','module->path = nil;'),
             'partial':('unload(load.module);','/* omitted partial numeric Module cleanup */'),
             'lease':('free64storage(load.lease);','/* omitted partial numeric lease cleanup */')}
    if control:
        old,new=changes[control]
        if parts.count(old)!=1: raise RuntimeError('numeric constructor control drift')
        parts=parts.replace(old,new,1)
    return source+(ROOT/'utils/tests/dis64-numeric-module.c').read_text().replace('NUMERIC_IMPLEMENTATION',parts)


# Keep storage proof/collector execution separate from the C/RX instruction execution fixture.
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--control',choices=('entry','path','partial','lease'));args=parser.parse_args()
    rows=cases();stream=W(len(rows))
    for name,accepted,raw,value,offset,width,error in rows:
        name=name.encode();stream+=W(len(name))+name+W(accepted,len(raw))+raw
    arch=os.environ.get('NATIVE64_ARCH','amd64');flags=shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes=['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='numeric-module-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);(work/'test.c').write_text(unit(args.control));(work/'fixtures').write_bytes(stream);binary=work/'test'
        for label,select in ([('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1'])] if args.control else [('WORD32',['-DDIS_WORD32=1']),('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']),('legacy',[])]):
            subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+includes+select+[str(work/'test.c'),'-o',str(binary)],check=True)
            result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(work/'fixtures')],capture_output=True,text=True,timeout=60)
            if args.control:
                stage={'entry':'numeric entry','path':'numeric constructor verdict','partial':'numeric partial baseline','lease':'root baseline'}[args.control]
                if result.returncode!=10 or result.stderr.strip()!='storage owner mismatch: '+stage: raise RuntimeError('numeric constructor control missed predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS numeric Module control:',args.control,arch)
            elif result.returncode: raise RuntimeError(result.stdout+result.stderr)
            else: print(label+': '+result.stdout,end='')


if __name__=='__main__':
    main()
