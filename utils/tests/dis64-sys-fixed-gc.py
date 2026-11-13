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
    source=source.replace('int\nkclose(int fd)', 'int\nbasekclose(int fd)',1)
    source=source.replace('long\nkread(int fd, void *buffer, long bytes)', 'long\nbasekread(int fd, void *buffer, long bytes)',1)
    source=source.replace('int\nkopen(char *path, int mode)', 'int\nbasekopen(char *path, int mode)',1)
    marker='&limbo_native64_Sys_remove_entry};'
    source=source.replace(marker,'&limbo_native64_Sys_remove_entry,&limbo_native64_Sys_millisec_entry,&limbo_native64_Sys_sleep_entry,&limbo_native64_Sys_dup_entry,&limbo_native64_Sys_unmount_entry,&limbo_native64_Sys_bind_entry,&limbo_native64_Sys_fildes_entry,&limbo_native64_Sys_iounit_entry,&limbo_native64_Sys_open_entry,&limbo_native64_Sys_create_entry,&limbo_native64_Sys_read_entry,&limbo_native64_Sys_write_entry,&limbo_native64_Sys_dial_entry};',1)
    source=source.replace('module->type=mallocz(3*sizeof(Type*),1);module->ext=mallocz(4*sizeof(Link),1);','module->type=mallocz(15*sizeof(Type*),1);module->ext=mallocz(16*sizeof(Link),1);').replace('for(i=0;i<3;i++){\n        entry=native64sysio[i];','for(i=0;i<15;i++){\n        entry=native64sysio[i];',1)
    source=source.replace('static void\ngrouprelease(void)\n{','static void\ngroupstringrelease(void)\n{',1)
    port=(ROOT/'emu/port/inferno.c').read_text()
    support='typedef struct FD FD;struct FD {Sys_FD fd;Fgrp *grp;};Type *TFD;static int fdclosing;\nint kiounit(int);\nint kunmount(char*,char*);int kbind(char*,char*,int);\nchar Ebadarg[]="bad argument";\n'+'\n'.join(extract(port,signature) for signature in ('static int\nsysnativeint(','void\nSys_millisec(','void\nSys_dup(','void\nSys_sleep(','void\nSys_unmount(','void\nSys_bind(','static void*\nsysresultowner(','static int\nsysiofd(','void\nfreeFD(','Sys_FD*\nmkfd(','void\nSys_fildes(','void\nSys_iounit(','void\nSys_open(','void\nSys_create(','static int\nsysiocount(','static void\nsysioframe(','void\nSys_read(','void\nSys_write('))
    support=extract((ROOT/'libinterp/heap.c').read_text(),'Heap*\nheap(')+'\n'+support
    start=port.index('typedef struct FDResult FDResult;');end=port.index('/* Mark a descriptor',start)
    support+='\nint kdial(char*,char*,char*,int*);int kclose(int);\n'+port[start:end]
    support+='\n'+'\n'.join(extract(port,sig) for sig in ('static Sys_FD*\ntakefd(', 'static void\nfreefdresult(', 'static uchar*\nconnectionresultslot(', 'static void\nclearconnection(', 'static void\nsysfdresultowners(', 'static void\nmakeconnection(', 'void\nSys_dial('))

    support=support.replace('handle = H2D(FD*, h);\n\n\trelease();','handle = H2D(FD*, h);\n\tfdclosing++;\n\trelease();').replace('closefgrp(handle->grp);\n\tacquire();','closefgrp(handle->grp);\n\tacquire();\n\tfdclosing--;')
    source=source.replace('void\nSys_werrstr(',support+'\nvoid\nSys_werrstr(',1)
    strings=(ROOT/'libinterp/string.c').read_text();heap=(ROOT/'libinterp/heap.c').read_text();xec=(ROOT/'libinterp/xec.c').read_text()
    source=source.replace(extract(source,'String* c2string('),extract(strings,'String*\nc2string('),1)
    arrays='Type Tbyte={1,0,0,1};char exType[]="type";\n'+'\n'.join(extract(heap,signature) for signature in ('void\nacheck(','void\ninitarray(','Heap*\nheaparray(','void\nfreearray('))
    arrays+='\n'+extract((ROOT/'libinterp/runt.c').read_text(),'Array*\nmem2array(')
    arrays+='\n'+'\n'.join(extract(xec,signature) for signature in ('static Type*\nmoduletype(','static void\nnewarray(','OP(newa)','OP(newaz)','OP(lena)','static uchar*\narrayspan(','OP(slicea)'))
    arrays+='\n'+'\n'.join(extract(strings,signature) for signature in ('OP(cvtca)','OP(cvtac)'))
    source+='\n'+arrays

    return source+'\n'+(ROOT/'utils/tests/dis64-sys-fixed-gc.c').read_text()+'\n'+(ROOT/'utils/tests/dis64-sys-dial-gc.c').read_text()


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
        rows=fixed['rows'](args.limbo,work);output=['typedef struct SysFixedEvent {const char *method,*text,*other;WORD first,second,status;} SysFixedEvent;','typedef struct SysFixedCase {const uchar *image;unsigned int bytes,imports;const char *names[14];unsigned int calls;SysFixedEvent events[16];const char *label;} SysFixedCase;']
        for i,(_,image,_) in enumerate(rows):output.append('static const uchar fixed_image'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const SysFixedCase sysfixed_cases[]={')
        for i,(label,image,events) in enumerate(rows):
            names,_,_=fixed['imports'](image)
            def event(e):
                name,text,first,second,status=e[:5];other=e[5] if len(e)>5 else None
                def quote(value):return '"'+value.replace('\\','\\\\').replace('"','\\"').replace('\0','\\000').replace('\n','\\n')+'"'
                return '{"'+name+'",'+(quote(text) if text is not None else '0')+','+('"'+other+'"' if other is not None else '0')+','+str(first)+','+str(second)+','+str(status)+'}'
            output.append('{fixed_image'+str(i)+',sizeof(fixed_image'+str(i)+'),'+str(len(names))+',{'+','.join('"'+name+'"' for name in names)+'},'+str(len(events))+',{'+','.join(event(e) for e in events)+'},"'+label+'"},')
        output.append('};');bad=list(fixed['refusals'](args.limbo,work))
        for i,image in enumerate(bad):output.append('static const uchar fixed_bad'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const struct {const uchar *image;unsigned int bytes;} sysfixed_bad[]={')
        for i in range(len(bad)):output.append('{fixed_bad'+str(i)+',sizeof(fixed_bad'+str(i)+')},')
        output.append('};');(work/'sys-fixed-images.h').write_text('\n'.join(output)+'\n')
        dial=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-dial.py'));dialrows=dial['rows'](args.limbo,work);dial['compiler_refusals'](args.limbo,work)
        output=['typedef struct DialEvent {const char *address,*local;} DialEvent;','typedef struct DialCase {const uchar *image;unsigned int bytes,calls;DialEvent events[32];} DialCase;']
        for i,(image,values) in enumerate(dialrows):output.append('static const uchar dial_image'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const DialCase dial_cases[]={')
        import json
        for i,(image,values) in enumerate(dialrows):output.append('{dial_image'+str(i)+',sizeof(dial_image'+str(i)+'),'+str(len(values))+',{'+','.join('{'+json.dumps(a,ensure_ascii=False)+','+json.dumps(b,ensure_ascii=False)+'}' for a,b in values)+'}},')
        output.append('};');dialbad=list(dial['refusals'](dialrows[0][0]))
        for i,image in enumerate(dialbad):output.append('static const uchar dial_bad'+str(i)+'[]={'+','.join(map(str,image))+'};')
        output.append('static const struct {const uchar *image;unsigned int bytes;} dial_bad[]={'+','.join('{dial_bad'+str(i)+',sizeof(dial_bad'+str(i)+')}' for i in range(len(dialbad)))+'};')
        (work/'sys-dial-images.h').write_text('\n'.join(output)+'\n')
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
        marker='flow[i] = (Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,0,40};'
        if header.count(marker)!=2:raise RuntimeError('pointer-fact control marker drift')
        (override/'dis64-sys-string.h').write_text(header.replace(marker,'flow[i] = (Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP),0,0,72};'))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'pointer-control')],check=True)
        result=subprocess.run(runner+[str(work/'pointer-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys fixed admission predicate':raise RuntimeError('pointer-fact control missed predicate: '+result.stdout+result.stderr)
        print('PASS active fixed Sys pointer-store fact pollution control',flush=True)
        (override/'dis64-sys-string.h').write_text(header)
        pilot=(ROOT/'include/dis64-pilot.h').read_text();marker='in->middle == -1 ? 32 : 8'
        if pilot.count(marker)!=1:raise RuntimeError('FD result control marker drift')
        (override/'dis64-pilot.h').write_text(pilot.replace(marker,'in->middle == -1 ? 40 : 8'))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'fd-result-control')],check=True)
        result=subprocess.run(runner+[str(work/'fd-result-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys fixed admission predicate':raise RuntimeError('FD result control missed predicate: '+result.stdout+result.stderr)
        print('PASS active fixed Sys FD-result-as-Word fact pollution control',flush=True)
        marker='kcreate(path, f->mode, (UWORD)f->perm)'
        if source.count(marker)!=1:raise RuntimeError('permission mask control marker drift')
        (work/'permission-control.c').write_text(source.replace(marker,'kcreate(path, f->mode, f->perm)'))
        subprocess.run(common+[str(work/'permission-control.c'),'-o',str(work/'permission-control')],check=True)
        result=subprocess.run(runner+[str(work/'permission-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=20 or result.stderr.strip()!='Sys open/create argument predicate':raise RuntimeError('permission mask control missed predicate: '+result.stdout+result.stderr)
        print('PASS active Sys create permission zero-extension removal control',flush=True)
        marker='*need=64; *define=64;'
        if pilot.count(marker)!=1:raise RuntimeError('slice fact control marker drift')
        (override/'dis64-pilot.h').write_text(pilot.replace(marker,'*need=0; *define=64;'))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'slice-control')],check=True)
        result=subprocess.run(runner+[str(work/'slice-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys fixed admission predicate':raise RuntimeError('slice array fact control missed predicate: '+result.stdout+result.stderr)
        print('PASS active slice array-initialization removal control',flush=True)
        marker='if(!bit || (bit&~15U))return 0;*need |= bit;'
        if pilot.count(marker)!=2:raise RuntimeError('slice bound control marker drift')
        (override/'dis64-pilot.h').write_text(pilot.replace(marker,'if(!bit || (bit&~15U))return 0;'))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'slice-bound-control')],check=True)
        result=subprocess.run(runner+[str(work/'slice-bound-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys fixed admission predicate':raise RuntimeError('slice bound fact control missed predicate: '+result.stdout+result.stderr)
        print('PASS active slice bound-initialization removal control',flush=True)
        marker='*define = dis64clockbit(in->destination); if(!*define) return 0;'
        if pilot.count(marker)!=1:raise RuntimeError('array result control marker drift')
        (override/'dis64-pilot.h').write_text(pilot.replace(marker,marker+' if(in->destination >= 112)*define |= 8;'))
        subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'array-result-control')],check=True)
        result=subprocess.run(runner+[str(work/'array-result-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=14 or result.stderr.strip()!='Sys fixed admission predicate':raise RuntimeError('array result control missed predicate: '+result.stdout+result.stderr)
        print('PASS active array-result-as-Word fact pollution control',flush=True)
        marker='if(load->bytearray){'
        if source.count(marker)!=1:raise RuntimeError('byte primitive control marker drift')
        (work/'byte-type-control.c').write_text(source.replace(marker,'if(0 && load->bytearray){'))
        subprocess.run(common+[str(work/'byte-type-control.c'),'-o',str(work/'byte-type-control')],check=True)
        result=subprocess.run(runner+[str(work/'byte-type-control')],capture_output=True,text=True,timeout=90)
        if result.returncode!=22 or result.stderr.strip()!='Sys byte primitive descriptor predicate':raise RuntimeError('byte primitive control missed predicate: '+result.stdout+result.stderr)
        print('PASS active native byte descriptor adoption removal control',flush=True)





        (override/'dis64-pilot.h').write_text(pilot)
        (override/'dis64-sys-string.h').write_text(header)
        dialheader=(ROOT/'include/dis64-sys-dial.h').read_text()
        for label,marker,replacement in (('tuple roots','frame.map[1]!=0xdc','0'),('tuple result address','in.source!=80','0')):
            if dialheader.count(marker)!=1:raise RuntimeError('dial control marker drift: '+label)
            (override/'dis64-sys-dial.h').write_text(dialheader.replace(marker,replacement))
            subprocess.run(common[:1]+['-I'+str(override)]+common[1:]+[str(work/'test.c'),'-o',str(work/'dial-control')],check=True)
            result=subprocess.run(runner+[str(work/'dial-control')],capture_output=True,text=True,timeout=90)
            if result.returncode!=24 or result.stderr.strip()!='Sys dial admission predicate':raise RuntimeError('dial control missed predicate '+label+': '+result.stdout+result.stderr)
            print('PASS active Sys dial '+label+' removal control',flush=True)


if __name__=='__main__':main()
