#!/usr/bin/env python3
"""Run the actual Android current-stack unwinder against the host's real unwind library."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
SOURCE=r'''
#include "dat.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
char calltracelog_path[32]="unused-test-path";
void *sig_ctx;
extern size_t captureBacktrace(void**,size_t,void*);


/* Supply the formatting boundary for the logger path, which this capture test does not execute. */
int snprint(char *out,int size,const char *format,...)
{
    int result;va_list ap;
    va_start(ap,format);result=vsnprintf(out,size,format,ap);va_end(ap);
    return result;
}


/* Exercise actual unwinding and ensure no native64 attempt interprets ARM32 ucontext fields. */
int main(void)
{
    void *frames[36];size_t count,capacity,i;
    if(captureBacktrace(NULL,32,NULL)!=0 || captureBacktrace(frames,0,NULL)!=0) return 1;
    if(captureBacktrace(frames,(size_t)-1,NULL)!=0) return 2;
    for(capacity=1;capacity<=32;capacity++){
        for(i=0;i<36;i++) frames[i]=(void*)(uintptr_t)0x55aa;
        count=captureBacktrace(frames+1,capacity,(void*)(uintptr_t)1);
        if(count==0 || count>capacity) return 3;
        if(frames[0]!=(void*)(uintptr_t)0x55aa) return 4;
        for(i=1+capacity;i<36;i++)
            if(frames[i]!=(void*)(uintptr_t)0x55aa) return 5;
    }
    puts("PASS native64 backtrace: actual unwinder, capacities 1..32, invalid spans and opaque signal context");
    return 0;
}
'''


# The test replaces declarations and logging transport only; actual _Unwind_Backtrace executes.
def main():
    with tempfile.TemporaryDirectory(prefix='native64-backtrace-',dir=ROOT) as directory:
        work=Path(directory)
        (work/'dat.h').write_text('#include <stdlib.h>\n#include <stdint.h>\n#include <sys/ucontext.h>\ntypedef unsigned long uintptr;\n#define nil NULL // Match the production null representation.\n')
        (work/'fns.h').write_text('int snprint(char*,int,const char*,...);\nvoid do_backtrace(char*);\n')
        (work/'test.c').write_text(SOURCE)
        subprocess.run(['cc','-D_GNU_SOURCE','-O3','-DNDEBUG','-fno-strict-aliasing','-funwind-tables','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(work),str(ROOT/'emu/Android/calltrace.c'),str(work/'test.c'),'-ldl','-o',str(work/'run')],check=True)
        subprocess.run([str(work/'run')],check=True)


if __name__=='__main__':
    main()
