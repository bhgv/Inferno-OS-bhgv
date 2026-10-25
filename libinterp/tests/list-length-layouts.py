#!/usr/bin/env python3
"""Check production list-length emitter layouts and staging/output bounds under sanitizers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='list-length-layouts-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64', 'riscv64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null sentinel used by private emitter helpers.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* This fixture allocates no heap blocks; process inspection is unavailable in the sandbox. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Vary private layouts independently and require atomic bounded publication. */
int
main(void)
{
    struct {
        u32int before; // Leading publication canary.
        u32int words[64]; // Actual caller capacity, with remaining words watched.
        u32int after; // Trailing publication canary.
    } output;
    LeafLayout a;
    int wb, big, field, value, cap, n, result, i, checks = 0;

    for(wb = 4; wb <= 8; wb += 4)
    for(field = 0; field < 5; field++) for(value = -8; value <= 4104; value++){
        memset(&a, 0, sizeof(a));
        a.wordbytes = wb; a.s = 8; a.d = 16; a.m = 24;
        a.listtail = 8;
        
        switch(field){
        case 0: a.listtail = value; break;
        case 1: a.s = value; break;
        case 2: a.m = value; break;
        case 3: a.d = value; break;
        case 4: a.pc = value; break;
        }
        n = leafemit(nil, 64, &a, ILENL);
        if(n < 0 || n > 64) abort();
        for(cap = 0; cap <= 64; cap += 8){
            memset(&output, 0xa5, sizeof(output));
            result = leafemit(output.words, cap, &a, ILENL);
            if(result != (n <= cap ? n : 0) || output.before != 0xa5a5a5a5 ||
               output.after != 0xa5a5a5a5) abort();
            for(i = result; i < 64; i++) if(output.words[i] != 0xa5a5a5a5) abort();
            checks++;
        }
    }
    printf("PASS list length layouts: %d sizing/publication/canary probes\\n", checks);
    return 0;
}
''')
        if arch == 'riscv64':
            source.write_text(source.read_text().replace('riscv64-emit.h', 'riscv-emit.h').replace('LeafLayout a;', 'RvLayout a;').replace('a.wordbytes = wb;', 'a.xlen = 64; a.wordbytes = wb;').replace('leafemit(', 'rvemit(').replace('for(i = result;', 'for(i = result == 0 && n > cap ? cap : result;'))
        binary = td/arch
        command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
