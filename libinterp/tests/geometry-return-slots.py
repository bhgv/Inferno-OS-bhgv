#!/usr/bin/env python3
"""Check all production geometry bindings with generated frames and byte-offset aliases."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('GEOMETRY_SOURCE', ROOT/'libinterp/geom.c')).read_text()
frames = (ROOT/'libinterp/runt.h').read_text()
geometry = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
names = re.findall(r'void\n((?:Point|Rect)_\w+)\(void \*fp\)', source)
if os.environ.get('GEOMETRY_RETURN_OP'):
    names = [name for name in names if name == os.environ['GEOMETRY_RETURN_OP']]
    if not names:
        raise ValueError('unknown geometry operation')
calls = []
for name in names:
    typename = 'F_'+name
    body = re.search(r'struct '+typename+r'\n\{(.*?)\n\};', frames, re.S).group(1)
    fields = re.findall(r'\n\s*(Draw_Point|Draw_Rect|WORD)\s+(\w+);', body)
    offsets = ','.join('__builtin_offsetof('+typename+','+field+')' for _, field in fields)
    sizes = ','.join('sizeof((('+typename+'*)0)->'+field+')' for _, field in fields)
    calls.append('{int offsets[]={'+offsets+'}, sizes[]={'+sizes+'};check('+name+',sizeof('+typename+'),__builtin_offsetof('+typename+',ret),sizeof(*(( '+typename+'*)0)->ret),offsets,sizes,'+str(len(fields))+');}')
unit = r'''
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "draw.h"
#include "runt.h"
#include "raise.h"
#include <stdio.h>
#include <assert.h>
#include <setjmp.h>
static jmp_buf boundary;
char exZdiv[]="zero divide";
static unsigned long checks;
/* Observe the VM exception without allowing partially published results. */
void error(char *message){assert(message==exZdiv);longjmp(boundary,1);}
/* No fixture allocation needs leak-process inspection. */
int __lsan_is_turned_off(void){return 1;}
IMPLEMENTATION
/* Isolate a production call so automatic test-loop state survives VM exceptions. */
static int invoke(void (*fn)(void*),void *frame)
{
    if(setjmp(boundary))return 0;
    fn(frame);return 1;
}
/* Compare separate aligned results with every partial input overlap and byte offset. */
static void check(void (*fn)(void*),int framesize,int retoffset,int outsize,int *offsets,int *sizes,int nargs)
{
    union { uvlong align; uchar bytes[1024]; } frame, reference;
    union { uvlong align; uchar bytes[128]; } output, result;
    uchar wantedframe[1024], wantedoutput[128], *destination;
    UWORD values[]={0,1,(UWORD)-1,(UWORD)1<<(sizeof(WORD)*8-1),((UWORD)1<<(sizeof(WORD)*8-1))-1,37,(UWORD)-91};
    UWORD value;
    int pattern,alias,offset,arg,k,ok,actual;
    assert(framesize<sizeof(frame.bytes) && outsize<64);
    for(pattern=0;pattern<64;pattern++)for(alias=0;alias<=nargs;alias++)for(offset=0;offset<8;offset++){
        memset(frame.bytes,0xa5,sizeof(frame.bytes));memset(output.bytes,0x5a,sizeof(output.bytes));
        for(arg=0;arg<nargs;arg++)for(k=0;k<sizes[arg]/sizeof(WORD);k++){
            value=values[(pattern+arg*3+k*5)%nelem(values)];
            if(pattern&8)value^=(UWORD)pattern*0x12345;
            memmove(frame.bytes+offsets[arg]+k*sizeof(WORD),&value,sizeof(value));
        }
        destination=alias ? frame.bytes+offsets[alias-1]+offset : output.bytes+16+offset;
        memmove(frame.bytes+retoffset,&destination,sizeof(destination));
        memmove(reference.bytes,frame.bytes,sizeof(frame.bytes));
        memmove(result.bytes,destination,outsize);
        {void *aligned=result.bytes;memmove(reference.bytes+retoffset,&aligned,sizeof(aligned));}
        ok=invoke(fn,reference.bytes);
        memmove(wantedframe,frame.bytes,sizeof(frame.bytes));memmove(wantedoutput,output.bytes,sizeof(output.bytes));
        if(ok){
            if(alias)memmove(wantedframe+offsets[alias-1]+offset,result.bytes,outsize);
            else memmove(wantedoutput+16+offset,result.bytes,outsize);
        }
        actual=invoke(fn,frame.bytes);assert(actual==ok);
        assert(!memcmp(frame.bytes,wantedframe,sizeof(frame.bytes)));
        assert(!memcmp(output.bytes,wantedoutput,sizeof(output.bytes)));checks++;
    }
}
/* Exercise the generated frame layout for every geometry entry point. */
int main(void)
{
CALLS
    printf("PASS geometry return slots: %lu alignment/partial-alias/error cases WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
'''.replace('IMPLEMENTATION', geometry).replace('CALLS', '\n'.join(calls))
if os.environ.get('GEOMETRY_RETURN_ALIGNED'):
    unit = unit.replace('offset<8', 'offset<1')
with tempfile.TemporaryDirectory(prefix='geometry-return-') as directory:
    td = Path(directory)
    target = td/'returns.c'
    target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        model = td/('word'+str(bits))
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('returns'+str(bits))
        subprocess.run(command+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
