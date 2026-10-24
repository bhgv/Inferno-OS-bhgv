#!/usr/bin/env python3
"""Exercise production emu/os exception handlers with guarded frames and heaps."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]

def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return source[start:brace+token.end()]
    raise ValueError(signature)

unit = r'''
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
REG R;
Type Tstring;
static Type othertype,frame,cleanup;
static Prog process;
static Module module;
static Modlink instance;
static Handler handlers[3];
static Except entries[2];
static Inst instructions[4];
static struct {Heap h;uchar data[128];} oldvalue,newvalue,exceptionvalue;
static uchar *storage,*fp;
static size_t framesize,headersize;
static char *diagnostic;
static int faildiag,diagnostics,newstrings,destroys,checks,alignment,observepublication;
#define BIT(bt,nb) ((bt)&(1<<(nb)))
#define NOPC 0xffffffff
#define FRTYPE(f) ((f)->t==nil ? SEXTYPE(f)->reg.TR : (f)->t)
static void *getowner(void*);
/* Heap ownership remains observable even when no allocator or GC is running. */
Prog *currun(void) {return &process;}
void destroy(void *p) {
    if(p==H)return;
    assert(p==oldvalue.data || p==newvalue.data || p==exceptionvalue.data);
    assert(D2H(p)->ref>0);
    if(observepublication && p==oldvalue.data)assert(getowner(fp+handlers[0].eoff)==H);
    D2H(p)->ref--;destroys++;
    if(p==exceptionvalue.data && D2H(p)->ref==0 && D2H(p)->t==&othertype &&
       getowner(exceptionvalue.data)==newvalue.data)destroy(newvalue.data);
}
WORD modstatus(REG *r,char *name,int n) {(void)r;assert(n>8);strcpy(name,"fixture");return 2;}
static String *newestring(char *message) {assert(!strcmp(message,"match"));newstrings++;return (String*)newvalue.data;}
/* A failed diagnostic resize must retain the old allocation and its contents. */
static void *diagnosticrealloc(void *p,size_t n) {
    assert(p==diagnostic);diagnostics++;
    if(faildiag)return nil;
    diagnostic=realloc(p,n);assert(diagnostic);return diagnostic;
}
int snprint(char *p,int n,char *format,...) {
    (void)format;assert(p==diagnostic && n>16);strcpy(p,"new diagnostic");return 14;
}
#define realloc diagnosticrealloc
IMPLEMENTATION
#undef realloc
/* Access fixture owner slots without reproducing the implementation's alignment bug. */
static void *getowner(void *slot) {void *p;memmove(&p,slot,sizeof(p));return p;}
static void putowner(void *slot,void *p) {memmove(slot,&p,sizeof(p));}
/* Allocate canaries around either a normal frame or a stack-extension frame. */
static void setup(int compiled,int extended,int payload,int clear,int failure,int last)
{
    Frame *f;Frame header={0};int slot,shift;
    memset(&R,0,sizeof(R));memset(&process,0,sizeof(process));
    memset(&module,0,sizeof(module));memset(&instance,0,sizeof(instance));
    memset(&frame,0,sizeof(frame));memset(&cleanup,0,sizeof(cleanup));
    memset(handlers,0,sizeof(handlers));memset(entries,0,sizeof(entries));
    memset(&oldvalue,0,sizeof(oldvalue));memset(&newvalue,0,sizeof(newvalue));memset(&exceptionvalue,0,sizeof(exceptionvalue));
    oldvalue.h.ref=newvalue.h.ref=exceptionvalue.h.ref=1;
    oldvalue.h.t=newvalue.h.t=&Tstring;
    exceptionvalue.h.t=payload==1?&Tstring:&othertype;
    framesize=sizeof(Frame)+4*sizeof(void*);
    headersize=extended?OA(Stkext,reg.tos.fu):0;
    shift=extended?0:alignment;
    storage=malloc(32+headersize+framesize+32+shift);assert(storage);memset(storage,0xa5,32+headersize+framesize+32+shift);
    fp=storage+32+headersize+shift;memset(fp,0,framesize);f=(Frame*)fp;
    frame.size=framesize;frame.np=(framesize/sizeof(void*)+7)/8;
    for(slot=sizeof(Frame)/sizeof(void*);slot<(int)(framesize/sizeof(void*));slot++)
        frame.map[slot/8]|=0x80>>(slot%8);
    cleanup=frame;header.t=extended?nil:&frame;memmove(fp,&header,sizeof(header));
    if(extended)SEXTYPE(f)->reg.TR=&frame;
    for(slot=sizeof(Frame);slot<(int)framesize;slot+=sizeof(void*))putowner(fp+slot,H);
    instance.m=&module;instance.prog=instructions;instance.compiled=compiled;
    module.prog=instructions;module.nprog=4;module.htab=handlers;
    handlers[0].pc1=0;handlers[0].pc2=2;
    handlers[0].eoff=last?framesize-sizeof(void*):sizeof(Frame);
    handlers[0].t=clear?&cleanup:nil;handlers[0].ne=payload==2?1:0;handlers[0].etab=entries;
    entries[0].s="match";entries[0].pc=compiled?4:2;
    entries[1].pc=NOPC;
    putowner(fp+handlers[0].eoff,oldvalue.data);
    process.exval=payload?exceptionvalue.data:H;process.kill="pending";
    diagnostic=malloc(32);assert(diagnostic);strcpy(diagnostic,"previous diagnostic");process.exstr=diagnostic;
    R.FP=fp;R.SP=fp+framesize;R.TS=R.SP;R.EX=storage+32;R.M=&instance;
    R.PC=compiled?(Inst*)((uchar*)instructions+1):instructions+1;
    diagnostics=newstrings=destroys=observepublication=0;faildiag=failure;
}
/* Guards include the extension's original registers and the entire frame header. */
static void finish(void)
{
    int i;
    for(i=0;i<32;i++)assert(storage[i]==0xa5 && fp[framesize+i]==0xa5);
    free(storage);free(diagnostic);
}
/* Check both pointer ownership and the copied process registers after a match. */
static void validhandlers(void)
{
    int compiled,extended,payload,clear,failure,last;
    Frame header;
    for(compiled=0;compiled<2;compiled++)for(extended=0;extended<2;extended++)
    for(payload=0;payload<3;payload++)for(clear=0;clear<2;clear++)
    for(failure=0;failure<2;failure++)for(last=0;last<2;last++){
        setup(compiled,extended,payload,clear,failure,last);memmove(&header,fp,sizeof(header));
        observepublication=!clear;
        assert(handler("match"));
        assert(!memcmp(&header,fp,sizeof(header)) && oldvalue.h.ref==0);
        assert(getowner(fp+handlers[0].eoff)==(payload?exceptionvalue.data:newvalue.data));
        assert((payload?exceptionvalue.h.ref:newvalue.h.ref)==1 && process.exval==H && process.kill==nil);
        assert(newstrings==!payload && diagnostics==1 && process.exstr==diagnostic);
        assert(!strcmp(process.exstr,failure?"previous diagnostic":"new diagnostic"));
        assert(R.PC==(compiled?(Inst*)((uchar*)instructions+4):instructions+2));
        assert(!memcmp(&process.R,&R,sizeof(R)));
        finish();checks++;
    }
}
/* Reject malformed metadata before diagnostics, frame cleanup or payload writes. */
static void invalidhandlers(void)
{
    int mode,extended;uchar saved[256];REG before;
    for(extended=0;extended<2;extended++)for(mode=0;mode<18;mode++){
        setup(0,extended,0,1,0,0);assert(framesize<=sizeof(saved));
        switch(mode){
        case 0:handlers[0].eoff=(ulong)-1;break;
        case 1:handlers[0].eoff=0;break;
        case 2:handlers[0].eoff=sizeof(Frame)-sizeof(void*);break;
        case 3:handlers[0].eoff=sizeof(Frame)+1;break;
        case 4:handlers[0].eoff=framesize;break;
        case 5:handlers[0].eoff=(ulong)INT_MAX+1;break;
        case 6:frame.np=-1;break;
        case 7:frame.np=INT_MAX;break;
        case 8:frame.map[0]=0;break;
        case 9:handlers[0].t=H;break;
        case 10:cleanup.size=framesize+sizeof(void*);break;
        case 11:cleanup.size=-1;break;
        case 12:cleanup.np=-1;break;
        case 13:cleanup.np=INT_MAX;break;
        case 14:cleanup.map[0]|=0x80;break;
        case 15:cleanup.size=sizeof(Frame)+1;break;
        case 16:frame.size=sizeof(Frame)-1;break;
        case 17:if(extended)SEXTYPE((Frame*)fp)->reg.TR=H;else {Type *bad=H;memmove(fp+O(Frame,t),&bad,sizeof(bad));}break;
        }
        memcpy(saved,fp,framesize);memcpy(&before,&R,sizeof(R));
        assert(!handler("match"));
        assert(!memcmp(saved,fp,framesize) && !memcmp(&before,&R,sizeof(R)));
        assert(oldvalue.h.ref==1 && process.exval==H && diagnostics==0 && newstrings==0);
        assert(process.exstr==diagnostic && !strcmp(diagnostic,"previous diagnostic"));
        finish();checks++;
    }
}
/* A bad earlier handler must not hide a later valid handler for the same PC. */
static void laterhandler(void)
{
    setup(0,0,0,0,0,0);handlers[1]=handlers[0];handlers[0].eoff=(ulong)-1;
    assert(handler("match") && getowner(fp+handlers[1].eoff)==newvalue.data);
    assert(oldvalue.h.ref==0 && diagnostics==1);finish();checks++;
}
/* Validate a caller before unwinding a normal frame, including string conversion. */
static void unwindhandlers(void)
{
    int compiled,payload,clear,i;uchar *expanded,*child;Frame header;
    for(compiled=0;compiled<2;compiled++)for(payload=0;payload<3;payload++)for(clear=0;clear<2;clear++){
        setup(compiled,0,payload,clear,0,0);
        expanded=malloc(32+2*framesize+32+2*alignment);assert(expanded);memset(expanded,0xa5,32+2*framesize+32+2*alignment);
        memcpy(expanded+32+alignment,fp,framesize);free(storage);storage=expanded;fp=storage+32+alignment;
        child=fp+framesize+alignment;memset(child,0,framesize);initmem(&frame,child);
        memset(&header,0,sizeof(header));header.t=&frame;header.fp=fp;
        header.lr=compiled?(Inst*)((uchar*)instructions+1):instructions+1;
        memmove(child,&header,sizeof(header));
        handlers[0].ne=0;
        if(payload==2)putowner(exceptionvalue.data,newvalue.data);
        R.FP=child;R.EX=storage+32;R.TS=R.SP=child+framesize;
        R.PC=compiled?(Inst*)((uchar*)instructions+3):instructions+3;
        assert(handler("match") && R.FP==fp && R.SP==child);
        assert(getowner(fp+handlers[0].eoff)==(payload==1?exceptionvalue.data:newvalue.data));
        assert(oldvalue.h.ref==0 && process.exval==H);
        if(payload==2)assert(exceptionvalue.h.ref==0 && newvalue.h.ref==1);
        assert(!memcmp(&process.R,&R,sizeof(R)));
        for(i=0;i<32;i++)assert(storage[i]==0xa5 && child[framesize+i]==0xa5);
        free(storage);free(diagnostic);checks++;
    }
}
/* Unwind and actually free an extension while retaining an unaligned caller. */
static void unwindextension(void)
{
    int compiled,payload,clear;Stkext *sx;Frame header={0};uchar *child;
    for(compiled=0;compiled<2;compiled++)for(payload=0;payload<3;payload++)for(clear=0;clear<2;clear++){
        setup(compiled,0,payload,clear,0,0);
        sx=calloc(1,sizeof(Stkext)+framesize);assert(sx);child=sx->reg.tos.fu;
        sx->reg.TR=&frame;sx->reg.SP=fp+framesize;sx->reg.TS=fp+framesize;sx->reg.EX=storage+32;
        initmem(&frame,child);header.fp=fp;
        header.lr=compiled?(Inst*)((uchar*)instructions+1):instructions+1;
        memmove(child,&header,sizeof(header));
        handlers[0].ne=0;
        if(payload==2)putowner(exceptionvalue.data,newvalue.data);
        R.FP=child;R.EX=(uchar*)sx;R.TS=R.SP=child+framesize;
        R.PC=compiled?(Inst*)((uchar*)instructions+3):instructions+3;
        assert(handler("match") && R.FP==fp && R.SP==fp+framesize && R.EX==storage+32);
        assert(getowner(fp+handlers[0].eoff)==(payload==1?exceptionvalue.data:newvalue.data));
        assert(oldvalue.h.ref==0 && process.exval==H && !memcmp(&process.R,&R,sizeof(R)));
        finish();checks++;
    }
}
/* The first diagnostic allocation may fail without preventing exception delivery. */
static void emptydiagnostic(void)
{
    int failure;
    for(failure=0;failure<2;failure++){
        setup(0,0,0,0,failure,0);free(diagnostic);diagnostic=process.exstr=nil;
        assert(handler("match") && process.exstr==diagnostic);
        if(failure)assert(process.exstr==nil);
        else assert(!strcmp(process.exstr,"new diagnostic"));
        assert(getowner(fp+handlers[0].eoff)==newvalue.data && oldvalue.h.ref==0);
        finish();checks++;
    }
}
/* Release an uncalled ordinary frame before publishing into its caller. */
static void uncalledframe(void)
{
    uchar *expanded,*temporary;Frame header={0};
    setup(0,0,0,0,0,0);
    expanded=malloc(32+2*framesize+32+alignment);assert(expanded);
    memset(expanded,0xa5,32+2*framesize+32+alignment);
    memcpy(expanded+32+alignment,fp,framesize);free(storage);storage=expanded;fp=expanded+32+alignment;
    temporary=fp+framesize;memset(temporary,0,framesize);initmem(&frame,temporary);
    header.t=&frame;memmove(temporary,&header,sizeof(header));
    oldvalue.h.ref++;putowner(temporary+sizeof(Frame),oldvalue.data);
    R.FP=fp;R.EX=storage+32;R.TS=R.SP=temporary+framesize;
    assert(handler("match") && R.FP==fp && R.SP==temporary);
    assert(oldvalue.h.ref==0 && getowner(fp+handlers[0].eoff)==newvalue.data);
    assert(getowner(temporary+sizeof(Frame))==oldvalue.data);
    for(int i=0;i<32;i++)assert(storage[i]==0xa5 && temporary[framesize+i]==0xa5);
    free(storage);free(diagnostic);checks++;
}
int main(void)
{
    char *only=getenv("EXCEPTION_CASES");
    for(alignment=0;alignment<8;alignment++){
    if(!only || !strcmp(only,"extension"))unwindextension();
    if(!only || !strcmp(only,"uncalled"))uncalledframe();
    if(!only || !strcmp(only,"valid"))validhandlers();
    if(!only || !strcmp(only,"invalid"))invalidhandlers();
    if(!only || !strcmp(only,"later"))laterhandler();
    if(!only || !strcmp(only,"unwind"))unwindhandlers();
    if(!only || !strcmp(only,"diagnostic"))emptydiagnostic();
    }
    printf("exception frames: %d cases\n",checks);return 0;
}
int __lsan_is_turned_off(void) {return 1;}
'''
heap = (root/'libinterp/heap.c').read_text()
for variant in ('emu','os'):
    if os.environ.get('EXCEPTION_VARIANT') not in (None,variant): continue
    folder = Path(os.environ.get('EXCEPTION_SOURCE_DIR', root/variant/'port'))
    source = (folder/'exception.c').read_text()
    signatures = ['static int\nematch(', 'static void\nfreeframe(']
    if 'static Type*\nframetype(' in source: signatures += ['static Type*\nframetype(']
    if 'static int\nexceptionframe(' in source: signatures += ['static int\nexceptionframe(']
    parts = [function(heap, s) for s in ('void\nfreeptrs(', 'void\ninitmem(')]
    parts += [function((root/'libinterp/stack.c').read_text(), 'void\nunextend(')]
    parts += [function(source, s) for s in signatures+['int\nhandler(']]
    with tempfile.TemporaryDirectory(prefix='exception-frames-') as directory:
        td=Path(directory);target=td/'test.c';target.write_text(unit.replace('IMPLEMENTATION','\n'.join(parts)))
        cmd=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        includes=['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        for model in ('native','word32'):
            override=[]
            if model=='word32':
                folder=td/'word32';folder.mkdir();header=(root/'include/interp.h').read_text()
                header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
                (folder/'interp.h').write_text(header);override=['-I'+str(folder)]
            binary=td/('test-'+model)
            subprocess.run(cmd+override+includes+[str(target),'-o',str(binary)],check=True)
            print(variant,model,flush=True);subprocess.run([str(binary)],check=True)
