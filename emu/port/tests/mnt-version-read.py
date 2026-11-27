# Copyright (c) 2026 bhgv
# EN: Exact production version-reader acceptance with deterministic fragmented transport.
# RU: Проверка настоящего version-reader с детерминированной фрагментацией транспорта.
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


# EN/RU: Isolate only the real bounded transport reader, preserving its source body.
# Изолировать только настоящий bounded transport reader без переписывания его тела.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--early-return-control', action='store_true')
    args = parser.parse_args()
    source = (Path(__file__).resolve().parents[3] / 'emu/port/devmnt.c').read_text()
    function = source[source.index('static long\nmntversionread('):source.index('/*\n * Version is not multiplexed:')]
    if args.early_return_control:
        function = function.replace('have += n;', 'have += n; return have;')
    before = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
#define BIT32SZ 4
#define BIT8SZ 1
#define BIT16SZ 2
#define GBIT32(p) ((uint32_t)(p)[0]|((uint32_t)(p)[1]<<8)|((uint32_t)(p)[2]<<16)|((uint32_t)(p)[3]<<24))
typedef struct { int type, l; uint64_t offset; } Chan;
static jmp_buf escape;
static uchar packet[64];
static uint available, used, chunk;
static int overread;
static void error(char *message) { (void)message; longjmp(escape,1); }
static void lock(int *p) { (void)p; }
static void unlock(int *p) { (void)p; }
static long transport(Chan *c, void *out, long wanted, uint64_t offset) {
    uint n=available-used;
    (void)c;
    if(offset!=7+used) { fputs("offset mismatch\n",stderr); exit(2); }
    if(overread) return wanted+1;
    if(n>(uint)wanted) n=wanted;
    if(n>chunk) n=chunk;
    memcpy(out,packet+used,n); used+=n; return n;
}
typedef struct { long (*read)(Chan*,void*,long,uint64_t); } Device;
static Device device={transport};
static Device *devtab[]={&device};
static void check(int valid) { if(!valid) { fputs("version transport mismatch\n",stderr); exit(2); } }
'''
    after = r'''
static void run(uint bytes, uint step, uint capacity, uint declared, int invalid, int oversized) {
    uchar output[64]; Chan c={0,0,7}; long n;
    available=bytes; used=0; chunk=step; overread=oversized;
    memset(packet,0,sizeof(packet));
    packet[0]=declared;packet[1]=declared>>8;packet[2]=declared>>16;packet[3]=declared>>24;
    if(setjmp(escape)) { check(invalid); return; }
    n=mntversionread(&c,output,capacity);
    check(!invalid && n==19 && used==19 && c.offset==26 && memcmp(output,packet,19)==0);
}
int main(void) {
    uint i;
    for(i=1;i<32;i++) run(38,i,32,19,0,0);
    for(i=0;i<19;i++) run(i,1,32,19,1,0);
    for(i=0;i<7;i++) run(32,1,32,i,1,0);
    run(32,1,32,33,1,0);run(32,1,32,0x80000000U,1,0);
    run(32,1,6,19,1,0);run(32,1,32,19,1,1);
    puts("PASS version reader: fragmented header/body, EOF prefixes, size limits, stream tail and offsets");
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='mnt-version-') as name:
        work = Path(name)
        (work / 'test.c').write_text(before + function + after)
        flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + flags + [str(work / 'test.c'), '-o', str(work / 'test')], check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER', '')) + [str(work / 'test')], capture_output=True, text=True)
        if args.early_return_control:
            if result.returncode != 2 or 'version transport mismatch' not in result.stderr:
                raise RuntimeError('early-return omission not detected: ' + result.stderr)
            print('PASS version reader early-return control')
        elif result.returncode:
            raise RuntimeError(result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
