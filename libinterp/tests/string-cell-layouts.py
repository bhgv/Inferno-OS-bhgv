#!/usr/bin/env python3
"""Check production string cell emitter layouts and staging/output bounds under sanitizers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='string-cell-layouts-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0 // Null pointer for sizing-only requests.
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
        u32int words[128]; // Actual caller capacity, with remaining words watched.
        u32int after; // Trailing publication canary.
    } output;
    LeafLayout a;
    int wb, opcode, field, value, cap, n, result, i, checks = 0;

    for(opcode = 0; opcode < 3; opcode++)
    for(wb = 4; wb <= 8; wb += 4)
    for(field = 0; field < 5; field++) for(value = -8; value <= 2056; value++){
        memset(&a, 0, sizeof(a));
        a.wordbytes = wb; a.s = 8; a.d = 16; a.m = 24;
        a.stringlen = 0; a.stringmax = 4; a.stringdata = 24;
        a.heapref = 24; a.refbytes = 8;
        switch(field){
        case 0: a.stringlen = value; break;
        case 1: a.stringmax = value; break;
        case 2: a.stringdata = value; break;
        case 3: a.heapref = value; break;
        case 4: a.refbytes = value; break;
        }
        n = leafemit(nil, 128, &a, (opcode == 0 ? IINDC : opcode == 1 ? IINSC : ISLICEC));
        if(n < 0 || n > 128) abort();
        for(cap = 0; cap <= 128; cap += 8){
            memset(&output, 0xa5, sizeof(output));
            result = leafemit(output.words, cap, &a, (opcode == 0 ? IINDC : opcode == 1 ? IINSC : ISLICEC));
            if(result != (n <= cap ? n : 0) || output.before != 0xa5a5a5a5 ||
               output.after != 0xa5a5a5a5) abort();
            for(i = result; i < 128; i++) if(output.words[i] != 0xa5a5a5a5) abort();
            checks++;
        }
    }
    printf("PASS string cell layouts: %d sizing/publication/canary probes\\n", checks);
    return 0;
}
''')
        binary = td/arch
        command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
