#!/usr/bin/env python3
"""Readonly, bounded diagnostic signature availability before legacy signing."""
import os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def main():
    extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    body=extract((ROOT/'limbo/dis64-typed.c').read_text(),'static int\ntypedsignable(')
    declarations=(ROOT/'limbo/decls.c').read_text();start=declarations.index('int storespace[Dend]');end=declarations.index('};',start)+2
    fixture=r'''
static void check(int ok,char *why){if(!ok){fprintf(stderr,"signature walk predicate: %s\n",why);exit(10);}}
int main(void){
 Type types[70]={{0}},saved[70];Type *active[64];Decl fields[3]={{0}},before[3];unsigned int budget,i;
 types[0].kind=Tfn;types[0].tof=types+1;types[1].kind=Tmodule;types[1].tof=types+2;types[2].kind=Tiface;
 memcpy(saved,types,sizeof(types));budget=4096;
 check(!typedsignable(types,active,0,&budget),"narrow Module");check(!memcmp(saved,types,sizeof(types)),"readonly narrowed types");
 types[2].linkall=1;budget=4096;check(typedsignable(types,active,0,&budget),"complete Module");
 types[2].linkall=0;types[3].kind=Tref;types[3].tof=types+4;types[4].kind=Tadt;types[4].ids=fields;
 fields[0].store=Dfn;fields[0].ty=types+1;budget=4096;
 check(typedsignable(types+3,active,0,&budget),"ADT methods are not fields");
 fields[0].store=Dfield;budget=4096;check(!typedsignable(types+3,active,0,&budget),"nested Module field");
 types[4].ids=nil;types[4].tags=fields;fields[0].store=Dtag;fields[0].ty=types+5;
 types[5].kind=Tadtpick;types[5].ids=fields+1;fields[1].store=Dfield;fields[1].ty=types+1;budget=4096;
 check(!typedsignable(types+3,active,0,&budget),"tagged Module field");
 types[4].tags=nil;types[4].ids=fields;fields[0].store=Dlocal;fields[0].ty=types+6;types[6].kind=Tint;budget=4096;
 check(!typedsignable(types+3,active,0,&budget),"invalid local signature field");
 types[3].kind=Tlist;types[3].tof=types+1;budget=4096;check(!typedsignable(types+3,active,0,&budget),"list Module");
 types[3].kind=Tref;types[3].tof=types+3;memcpy(saved,types,sizeof(types));budget=4096;
 check(typedsignable(types+3,active,0,&budget)&&!memcmp(saved,types,sizeof(types)),"recursive reference readonly");
 memset(types,0,sizeof(types));for(i=0;i<64;i++){types[i].kind=Tref;types[i].tof=types+i+1;}types[63].kind=Tint;types[63].tof=nil;
 budget=4096;check(typedsignable(types,active,0,&budget),"depth64");
 types[63].kind=Tref;types[63].tof=types+64;types[64].kind=Tint;budget=4096;
 check(!typedsignable(types,active,0,&budget),"depth65 refused");
 types[0].kind=Tref;types[0].tof=types+1;types[1].kind=Tint;budget=1;
 check(!typedsignable(types,active,0,&budget),"visit budget");
 types[0].kind=Tadt;types[0].ids=fields;fields[0].store=Dfn;fields[0].next=fields;
 memcpy(before,fields,sizeof(fields));memcpy(saved,types,sizeof(types));budget=32;
 check(!typedsignable(types,active,0,&budget)&&!budget,"filtered Decl cycle bounded");
 check(!memcmp(before,fields,sizeof(fields))&&!memcmp(saved,types,sizeof(types)),"readonly cyclic metadata");
 puts("PASS diagnostic signatures: narrowed/full/nested Modules, method filtering, readonly recursion, depth64/65 and bounded Decl cycles");return 0;
}
'''
    prefix='#include "limbo.h"\n#include <stdio.h>\n'+declarations[start:end]+'\n'
    cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    cc+=['-I'+str(ROOT/p)for p in ('Linux/386/include','include','limbo')];runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
    with tempfile.TemporaryDirectory(prefix='typed-signature-',dir=ROOT)as td:
        w=Path(td);src=w/'test.c';binary=w/'test';command=cc+[str(src),'-o',str(binary)]
        src.write_text(prefix+body+'\n'+fixture);subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=30)
        for old,new,diagnostic in [('!t->tof || !t->tof->linkall','!t->tof','narrow Module'),('depth==64','depth==63','depth64')]:
            assert body.count(old)==1;src.write_text(prefix+body.replace(old,new,1)+'\n'+fixture);subprocess.run(command,check=True)
            r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30)
            assert r.returncode==10 and 'signature walk predicate: '+diagnostic in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
            print('PASS signature active control:',diagnostic,flush=True)
if __name__=='__main__':main()
