#!/usr/bin/env python3
"""Sweep payload cons private layouts and caller capacities under ASan/UBSan."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='payload-cons-layouts-') as temporary:
    td = Path(temporary)
    for arch in ('amd64', 'arm64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null identity for private emission helpers.
#include "isa.h"
#include HEADER


/* All fixture storage is released; process-based leak inspection is unavailable. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Sweep offsets, type identities and code capacities without publishing partial leaves. */
int
main(void)
{
    struct {
        u32int before; // Leading output canary.
        u32int words[128]; // Caller-owned code buffer, independent of private staging size.
        u32int after; // Trailing output canary.
    } output;
    LeafLayout a;
    int op, operation, wb, field, value, cap, n, result, i;
    unsigned long checks;

    checks = 0;
    for(op = 0; op < 2; op++)
    for(wb = 4; wb <= 8; wb += 4)
    for(field = 0; field < 28; field++)
    for(value = -8; value <= 2056; value++){
        memset(&a, 0, sizeof(a));
        a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
        a.arraylen=0; a.arraytype=8; a.arrayroot=16; a.arraydata=24;
        a.typesize=24; a.typenp=28;
        a.stringlen=0; a.stringmax=4; a.stringdata=24;
        a.heapref=24; a.heaptype=16; a.heapcolor=32; a.refbytes=8;
        a.module=40; a.linkmodule=8; a.moduletypes=32; a.modulenty=40;
        a.typefree=8; a.typemark=16; a.plainfree=(void*)0x12345678; a.plainmark=(void*)0x87654321;
        a.arrayidentity=(void*)0x12345678; a.stringtype=(void*)0x87654321;
        a.listtail=0; a.listtype=8; a.listdata=16; a.typeref=0;
        a.consentry=(void*)0x123456780000ULL; a.ptrtype=(void*)0x100004100ULL;
        a.mutator=(void*)0x100004020ULL; a.nprop=(void*)0x100004024ULL; a.markcolor=3;
        switch(field){
        case 0: a.wordbytes=value; break;
        case 1: a.listdata=value; break;
        case 2: a.listtype=value; break;
        case 3: a.listtail=value; break;
        case 4: a.typeref=value; break;
        case 5: a.heapref=value; break;
        case 6: a.heapcolor=value; break;
        case 7: a.refbytes=value; break;
        case 8: a.markcolor=value; break;
        case 9: a.s=value; break;
        case 10: a.d=value; break;
        case 11: a.m=value; break;
        case 12: a.pc=value; break;
        case 13: a.consentry=nil; break;
        case 14: a.consentry=(void*)(uintptr_t)-1; break;
        case 15: a.ptrtype=nil; break;
        case 16: a.ptrtype=(void*)(uintptr_t)-1; break;
        case 17: a.mutator=nil; break;
        case 18: a.nprop=nil; break;
        case 19: a.ptrtype=(void*)((uintptr_t)-4); break;
        case 20: a.mutator=(void*)(uintptr_t)-1; break;
        case 21: a.nprop=(void*)(uintptr_t)-1; break;
        case 22: a.nprop=a.mutator; break;
        case 23: a.listtype=a.listdata; break;
        case 24: a.ptrtype=(void*)0x100004104ULL; break;
        case 25: a.mutator=(void*)0x100004023ULL; break;
        case 26: a.mutator=(void*)((uintptr_t)a.ptrtype+a.typeref); break;
        case 27: a.listdata=2040; break;
        }
        operation=op==0 ? ICONSM : ICONSP;
        n = leafemit(nil, 128, &a, operation);
        if(n < 0 || n > 128)
            abort();
        for(cap = 0; cap <= 128; cap += 8){
            memset(&output, 0xa5, sizeof(output));
            result = leafemit(output.words, cap, &a, operation);
            if(result != (n <= cap ? n : 0) || output.before != 0xa5a5a5a5 || output.after != 0xa5a5a5a5)
                abort();
            for(i = result; i < 128; i++)
                if(output.words[i] != 0xa5a5a5a5)
                    abort();
            checks++;
        }
    }
    printf("PASS payload cons layouts: %lu sizing/publication/canary cases\\n", checks);
    return 0;
}
'''.replace('HEADER', '"'+arch+'-emit.h"'))
        binary = td/(arch+'-layouts')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
