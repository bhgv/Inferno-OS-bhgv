#!/usr/bin/env python3
"""Run production formatting with reduced address guards and native payloads."""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]  # Owning source tree.
parts=[]
for name in ('sprint','vsnprint','vseprint'):
    source=Path(os.environ.get('OUTPUT_'+name.upper()+'_SOURCE',ROOT/'lib9'/(name+'.c'))).read_text()
    # Reduce arithmetic only; all formatter pointers and payloads remain native.
    for value in ('buf','e','fmt'):
        source=source.replace('(uintptr)'+value,'(FormatAddress)(uintptr)'+value)
    source=source.replace('(uintptr)-1','(FormatAddress)-1')
    source=re.sub(r'^#include "[^"\n]+"\n','',source,flags=re.M)
    parts.append(source)
prefix='#include "lib9.h"\n#include "fmtdef.h"\ntypedef unsigned short FormatAddress; // Exhaustible 16-bit guard coordinates.\n'
fixture=r'''
#include <stdio.h>
static int invoked;


/* Keep every side-effect and complete image oracle with NDEBUG. */
static void
check(int condition)
{
    if(!condition)abort();
}


/* Custom handlers make premature formatter dispatch observable. */
static int
payload(Fmt *format)
{
    invoked++;
    return fmtstrcpy(format,"0123456789abcdefXYZ");
}


/* Link the real error formatter without a running VM. */
void
rerrstr(char *p, uint n)
{
    if(n)snprintf(p,n,"fixture");
}


/* Process inspection is unavailable; all owned allocations are explicitly released. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Call the real va_list formatter without depending on host libc formatting. */
static int
bounded(char *p, int n, char *format, ...)
{
    int result;
    va_list args;
    va_start(args,format);
    result=vsnprint(p,n,format,args);
    va_end(args);
    return result;
}


/* Endpoint arithmetic and varargs copying are independently observable. */
static char*
endpoint(char *p, char *end, char *format, ...)
{
    char *result;
    va_list args;
    va_start(args,format);
    result=vseprint(p,end,format,args);
    va_end(args);
    return result;
}


/* Compare rejection, exact truncation/NUL and untouched neighboring bytes. */
int
main(void)
{
    uchar *allocation=malloc(3*65536), *base, *p;
    uchar expected[256];
    char *format, *endresult;
    int positions[]={1,7,31,32768,65024,65407,65471,65500,65503,65504,65505,65515,65520,65528,65532,65533,65534,65535};
    int lengths[]={-1,0,1,2,3,7,8,15,16,19,20,21,31,32,64,127,129};
    int mode,pi,li,n,accepted,written,result,low;
    unsigned long checks=0;

    check(allocation != nil);
    base=(uchar*)(((uintptr)allocation+65535)&~(uintptr)65535);
    format=(char*)base+512;
    memcpy(format,"%Z",3);
    check(fmtinstall('Z',payload)==0);
    for(mode=0;mode<3;mode++)for(pi=0;pi<18;pi++)for(li=0;li<17;li++){
        p=base+positions[pi];low=positions[pi];n=lengths[li];
        if(mode==0)n=65535-low;
        accepted=low!=65535 && n>0 && low+n<=65535;
        if(mode==2)accepted=low+(n>0?n:0)<=65535 && n>0;
        memset(p,0xa5,256);memset(expected,0xa5,sizeof(expected));
        written=accepted ? (n-1<19?n-1:19) : 0;
        if(accepted){memcpy(expected,"0123456789abcdefXYZ",written);expected[written]=0;}
        invoked=0;
        if(mode==0)result=sprint((char*)p,format);
        else if(mode==1)result=bounded((char*)p,n,format);
        else{
            endresult=endpoint((char*)p,(char*)p+n,format);
            result=endresult==nil ? -1 : endresult-(char*)p;
        }
        check(accepted ? result==written && invoked==1 : result==-1 && invoked==0);
        check(!memcmp(p,expected,sizeof(expected)));
        checks++;
    }
    invoked=0;
    check(sprint(nil,format)==-1 && bounded(nil,32,format)==-1 && endpoint(nil,(char*)base+32,format)==nil);
    check(bounded((char*)base+1024,32,nil)==-1 && endpoint((char*)base+1024,(char*)base+1056,nil)==nil);
    check(invoked==0);
    free(allocation);
    printf("PASS native output spans: %lu dispatch/truncation/NUL/image cases and five nil preflights\n",checks);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='native-output-spans-') as directory:
    td=Path(directory);target=td/'output.c';target.write_text(prefix+'\n'.join(parts)+fixture)
    binary=td/'output';command=shlex.split(os.environ.get('CC','cc'))
    command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command+=['-I'+str(ROOT/p) for p in ('Linux/386/include','include','lib9')]
    names=('rune','snprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','dorfmt')
    command+=[str(target)]+[str(ROOT/'lib9'/(name+'.c')) for name in names]
    subprocess.run(command+['-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
