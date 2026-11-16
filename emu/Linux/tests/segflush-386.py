#!/usr/bin/env python3
"""Check actual Linux/386 code publication, exact pages, overflow and OS failure."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
TEST = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
extern int segflush(void*, unsigned long);
static void check(int ok) { if(!ok) { fputs("386 code publication mismatch\n",stderr); exit(10); } }
static void permissions(void *address, const char *expected)
{
    FILE *file=fopen("/proc/self/maps","r");
    char line[512], flags[5]; unsigned long start,end;
    check(file!=NULL);
    while(fgets(line,sizeof(line),file)) {
        if(sscanf(line,"%lx-%lx %4s",&start,&end,flags)==3 &&
           (uintptr_t)address>=start && (uintptr_t)address<end) {
            check(!strncmp(flags,expected,3)); fclose(file); return;
        }
    }
    check(0);
}
int main(void)
{
    long page=sysconf(_SC_PAGESIZE);
    unsigned char *mapping,*code;
    const unsigned char returns42[]={0xb8,42,0,0,0,0xc3};
    check(page>0);
    mapping=mmap(NULL,3*page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    check(mapping!=MAP_FAILED);
    code=mapping+page+3;
    check(!mprotect(mapping+page,page,PROT_READ|PROT_WRITE));
    memcpy(code,returns42,sizeof(returns42));
    permissions(code,"rw-");
    check(segflush(NULL,0)==0 && segflush(NULL,1)!=0);
    check(segflush((void*)(UINTPTR_MAX-3),8)!=0);
    check(segflush((void*)(UINTPTR_MAX-1),1)!=0);
    check(!segflush(code,sizeof(returns42)));
    permissions(mapping,"---"); permissions(code,"rwx");
    permissions(mapping+2*page,"---");
    check(((int(*)(void))code)()==42);
    code[1]=43; check(((int(*)(void))code)()==43);
    check(!munmap(mapping+page,page));
    check(segflush(code,sizeof(returns42))!=0);
    check(!munmap(mapping,3*page));
    puts("PASS Linux/386 code publication: exact page, actual execution, writable literals, overflow and unmapped refusal");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--noexec-control', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='segflush-386-') as directory:
        work = Path(directory)
        source, binary = work/'test.c', work/'test'
        source.write_text(TEST)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O2 -g -Wall -Wextra'))
        command += ['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include')]
        implementation = ROOT/'emu/Linux/segflush-386.c'
        if args.noexec_control:
            text = implementation.read_text()
            marker = 'PROT_READ|PROT_WRITE|PROT_EXEC'
            if text.count(marker) != 1:
                raise RuntimeError('386 executable-page control marker drift')
            implementation = work/'segflush-control.c'
            implementation.write_text(text.replace(marker, 'PROT_READ|PROT_WRITE', 1))
        subprocess.run(command+[str(source), str(implementation),
                                '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
        if args.noexec_control:
            if result.returncode != 10 or result.stderr.strip() != '386 code publication mismatch':
                raise RuntimeError('Missing execute permission was not detected: '+result.stdout+result.stderr)
            print('PASS Linux/386 executable permission omission control')
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
