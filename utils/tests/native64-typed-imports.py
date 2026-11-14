#!/usr/bin/env python3
"""Construct owned client imports and execute linked source modules."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--materialize-only',action='store_true');args=p.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
    with tempfile.TemporaryDirectory(prefix='typed-imports-',dir=ROOT)as td:
        work=Path(td);env['TMPDIR']=str(work)
        runpy.run_path(str(ROOT/'utils/tests/dis64-typed-module.py'))['generate'](args.limbo.resolve(),work)
        runpy.run_path(str(ROOT/'utils/tests/dis64-typed-imports.py'))['generate'](args.limbo.resolve(),work)
        support=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'));extract=support['function'];xec=(ROOT/'libinterp/xec.c').read_text();link=(ROOT/'libinterp/link.c').read_text()
        source=runpy.run_path(str(ROOT/'utils/tests/native64-local-calls.py'))['unit']().split('/* Keep scalar file admission separate')[0]
        stub='void\ndestroylinks(Module *module)\n{\n\trequire(module->ext == nil);\n}'
        assert source.count(stub)==1;source=source.replace(stub,extract(link,'void\ndestroylinks('))
        source+='''
#include <stdarg.h>
static int rootcount;
typedef struct Pin Pin;struct Pin{Heap*h;Pin*next;};static Pin *pins;
void ptradd(Heap*h){Pin*p=mallocz(sizeof(*p),1);if(p==nil)error(exNomem);p->h=h;p->next=pins;pins=p;rootcount++;}
void ptrdel(Heap*h){Pin**p=&pins,*old;while(*p && (*p)->h!=h)p=&(*p)->next;require(*p!=nil);old=*p;*p=old->next;free(old);rootcount--;}
int snprint(char*s,int n,char*f,...){int r;va_list ap;va_start(ap,f);r=vsnprintf(s,n,f,ap);va_end(ap);return r;}
int print(char*f,...){USED(f);return 0;}
Prog*currun(void){require(0);return nil;}
Array*arraycpy(Array*a){USED(a);require(0);return nil;}
#include "native64-local-bind.h"
'''
        source+=extract(xec,'int\nnative64typedlea8step(')+'\n'
        for sig in ('static int\nmoduleentry(','OP(mframe)','OP(mcall)','OP(movb)'):source+=extract(xec,sig)+'\n'
        source+=extract(link,'static int\nlinkm(')+'\n'+extract((ROOT/'libinterp/heap.c').read_text(),'void\nnewmp(')+'\n'
        source+='\ntypedef struct LinkModule LinkModule;\n'+extract(link,'struct LinkModule {')+';\n'
        source+='void newdyndata(Modlink *m){USED(m);require(0); }\n'+extract((ROOT/'emu/port/dis.c').read_text(),'void\nkrethrow(')+'\n'
        for sig in ('static int\nlinkmodule(', 'Modlink*\nlinkmod('):source+=extract(link,sig)+'\n'
        source+='\n#define main moduleoldmain\n'+(ROOT/'utils/tests/native64-typed-module.c').read_text()+'\n#undef main\n#define main externaloldmain\n'+(ROOT/'utils/tests/native64-typed-external.c').read_text()+'\n#undef main\n#line 1 "native64-typed-imports.c"\n'+(ROOT/'utils/tests/native64-typed-imports.c').read_text();file=work/'module.c';file.write_text(source)
        command=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        if arch=='arm64':command+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch=='riscv64':command+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
        elif arch!='amd64':raise RuntimeError(arch)
        binary=work/'module';command+=['-DDIS_WORD32=1']+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        command+=[str(file),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)]
        runner=shlex.split(env.get('NATIVE64_RUNNER',''));runargs=['--materialize-only']if args.materialize_only else []
        subprocess.run(command,check=True,env=env);subprocess.run(runner+[str(binary)]+runargs,check=True,env=env,timeout=120)
        # Deliberate omissions must produce their exact semantic diagnostics.
        production=(ROOT/'include/native64-typed-module.h').read_text()
        for before,after,diagnostic in (('site->table>=s->ntables','0','case=0'),('!(map[bit/8]&(0x80>>(bit%8)))','0','case=3')):
            assert production.count(before)==1
            (work/'native64-typed-module.h').write_text(production.replace(before,after))
            subprocess.run(command,check=True,env=env)
            result=subprocess.run(runner+[str(binary)]+runargs,capture_output=True,text=True,env=env,timeout=120)
            assert result.returncode==10 and ('import constructor guard removed: '+diagnostic)in result.stderr,result.stderr
            assert 'runtime error:'not in result.stderr and 'Sanitizer'not in result.stderr,result.stderr
            print('PASS import constructor predicate control:',arch,diagnostic,flush=True)
        (work/'native64-typed-module.h').unlink()
        before='if(l->sig != sig)';assert source.count(before)==1
        file.write_text(source.replace(before,'if(0)'));subprocess.run(command,check=True,env=env)
        result=subprocess.run(runner+[str(binary)]+runargs,capture_output=True,text=True,env=env,timeout=120)
        assert result.returncode==10 and 'import binding guard removed: case=1'in result.stderr,result.stderr
        assert 'runtime error:'not in result.stderr and 'Sanitizer'not in result.stderr,result.stderr
        print('PASS owned import exact hash predicate control:',arch,flush=True)
if __name__=='__main__':main()
