#!/usr/bin/env python3
"""Ensure a future classifier kind cannot silently publish a native no-op."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
CLASSIFIER=(ROOT/'libinterp/leaf64-ops.h').read_text()
MARKER='case IRUNT: p->kind = LeafNop; break;'
assert CLASSIFIER.count(MARKER)==1
UNIT=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "emitter.h"
/* A sizing rejection must preserve the entire caller-owned publication buffer. */
int
main(void)
{
    LeafLayout abi={0};
    u32int out[LeafMax],before[LeafMax];
    int word,n;
    abi.pc=0;abi.s=8;abi.d=16;abi.m=24;
    for(word=4;word<=8;word+=4){
        abi.wordbytes=word;
        memset(out,0xa5,sizeof(out));memmove(before,out,sizeof(out));
        n=leafemit(out,LeafMax,&abi,IRUNT);
        if(UNKNOWN){assert(n==0 && !memcmp(out,before,sizeof(out)));}
        else{assert(n>0 && n<=LeafMax);assert(leafemit(0,LeafMax,&abi,IRUNT)==n);}
    }
    printf("PASS unknown leaf kind: %s classifier, WORD32/64\n",UNKNOWN ? "rejected" : "ordinary nop");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='unknown-leaf-kind-') as directory:
    td=Path(directory)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    # No allocations survive the process; retain sanitizer checks without unavailable LSan inspection.
    unit=UNIT.replace('#include "emitter.h"','#include "emitter.h"\n/* Preserve sanitizer instrumentation without leak-process inspection. */\nint\n__lsan_is_turned_off(void)\n{\n    return 1;\n}')
    for arch in ('amd64','arm64'):
        for unknown in (0,1):
            model=td/(arch+str(unknown))
            model.mkdir()
            emitter=Path(os.environ.get('UNKNOWN_'+arch.upper()+'_EMITTER',ROOT/'libinterp'/ (arch+'-emit.h'))).read_text()
            (model/'emitter.h').write_text(emitter)
            (model/'leaf64-ops.h').write_text(CLASSIFIER.replace(MARKER,'case IRUNT: p->kind = 777; break;') if unknown else CLASSIFIER)
            target=model/'probe.c'
            target.write_text(unit)
            binary=model/'probe'
            subprocess.run(command+['-DUNKNOWN='+str(unknown),'-I'+str(model),'-I'+str(ROOT/'include'),str(target),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
