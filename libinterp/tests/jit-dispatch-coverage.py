#!/usr/bin/env python3
"""Measure complete tail-entry availability independently of guarded fast code."""
import importlib.util
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning source tree.


# Validate both nil/non-nil fast targets, every capacity, and inactive opcode rejection.
def measure(arch, names, directory):
    source = directory/'dispatch-coverage.c'
    source.write_text('''#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // A null output requests sizing only.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Measure complete dispatch without executing target-architecture instructions. */
int
main(void)
{
    u32int out[LeafMax+2];
    void *entry=(void*)0x100009000ULL;
    void *fast;
    int wb, op, mode, n, cap, k, result, active;

    printf("%d\\n",MAXDIS);
    for(wb=4; wb<=8; wb+=4)
        for(op=0; op<256; op++){
            active=op>INOP && op<MAXDIS && op!=IECLR;
            n=leafbridgeemit(nil,LeafMax,entry,op,nil);
            if((n>0)!=active || n>LeafMax)
                return 2;
            for(mode=0; mode<2; mode++){
                fast=mode ? (void*)0x123456789abcde00ULL : nil;
                if(leafbridgeemit(nil,LeafMax,entry,op,fast)!=n)
                    return 3;
                for(cap=0; cap<=LeafMax; cap++){
                    memset(out,0xa5,sizeof(out));
                    result=leafbridgeemit(out+1,cap,entry,op,fast);
                    if(result!=(n && cap>=n ? n : 0) || out[0]!=0xa5a5a5a5U)
                        return 4;
                    for(k=result+1; k<LeafMax+2; k++)
                        if(out[k]!=0xa5a5a5a5U)
                            return 5;
                }
                if(leafbridgeemit(nil,LeafMax,nil,op,fast)!=0 ||
                   leafbridgeemit(nil,LeafMax,(void*)-1,op,fast)!=0 ||
                   leafbridgeemit(nil,LeafMax,entry,op,(void*)-1)!=0)
                    return 6;
            }
            if(active)
                printf("%d %d\\n",wb,op);
        }
    return 0;
}
''')
    binary = directory/'dispatch-coverage'
    command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(command+['-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
    lines = subprocess.check_output([str(binary)],text=True).splitlines()
    if not lines or int(lines[0])!=len(names): raise RuntimeError('opcode enum mismatch')
    result = {4: [],8: []}
    for line in lines[1:]:
        wb,op = map(int,line.split())
        if wb not in result or not 0<op<len(names) or names[op]=='IECLR' or names[op] in result[wb]: raise RuntimeError('invalid dispatch report')
        result[wb].append(names[op])
    return result


# Print independent per-architecture, per-WORD complete-entry availability.
def main():
    spec = importlib.util.spec_from_file_location('coverage',ROOT/'libinterp/tests/jit-coverage.py')
    coverage = importlib.util.module_from_spec(spec)
    exec(compile((ROOT/'libinterp/tests/jit-coverage.py').read_bytes(),
                 str(ROOT/'libinterp/tests/jit-coverage.py'), 'exec'), coverage.__dict__)
    names = coverage.opnames()
    with tempfile.TemporaryDirectory(prefix='jit-dispatch-coverage-') as temporary:
        for arch in ('amd64','arm64'):
            result = measure(arch,names,Path(temporary))
            for wb,available in result.items():
                print('PASS',arch,'WORD'+str(wb*8),'complete entries:',str(len(available))+'/'+str(len(names)-2))


if __name__=='__main__':
    main()
