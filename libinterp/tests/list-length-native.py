#!/usr/bin/env python3
"""Compare C and published AMD64 list lengths, including cycle rejection."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

if platform.machine().lower() not in ('x86_64', 'amd64'):
    raise RuntimeError('native raw-copy execution requires an x86-64 host')
ROOT = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('LIST_LENGTH_SOURCE', ROOT/'libinterp/xec.c')).read_text()


# Ignore comments and literals while extracting complete production C bodies.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [function(signature) for signature in ('OP(lenl)',)]
unit = (ROOT/'libinterp/tests/list-length-native.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='list-length-native-') as directory:
    td = Path(directory)
    target = td/'copies.c'; target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (4, 8):
        if os.environ.get("LIST_LENGTH_COUNTER_ONLY"):continue
        model = td/str(bits); model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 4:
            header, words = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, unsigned = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if words != 1 or unsigned != 1:raise RuntimeError('WORD32 header mismatch')
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/name) for name in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('copy'+str(bits))
        subprocess.run(command+includes+[str(target), '-o', str(binary), '-lm'], check=True)
        subprocess.run([str(binary)], check=True, timeout=3 if os.environ.get('LIST_LENGTH_CYCLE') else 60)
    # Reach the production counter limit using a reduced arithmetic model, not a new VM ABI.
    counter = td/'counter.c'
    counter.write_text('''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stddef.h>
typedef uintptr_t uintptr;
typedef unsigned char uchar;
typedef uint8_t UWORD;
typedef int8_t WORD;
typedef struct List List;

struct List {
    List *tail; // Borrowed next link; the reduced model exercises only count arithmetic.
};
#define nil 0 // Native null identity.
#define H ((void*)-1) // Interpreter nil-owner sentinel.
#define OP(name) void name(void) // Match the extracted production entry signature.
static struct {
    void *s; // Borrowed owner slot.
    void *d; // Borrowed length-result slot.
} R;
static jmp_buf boundary;
static char *raised;
static char exHeap[]="heap", exBounds[]="bounds", exNilref[]="nil";


/* Deliver production validation errors into one independent arithmetic-model check. */
void error(char *message)
{
    raised=message;
    longjmp(boundary,1);
}


/* This fixture allocates nothing and needs no unavailable process-based leak inspection. */
int __lsan_is_turned_off(void)
{
    return 1;
}
'''+parts[0]+'''


/* Reach the final legal count and first rejected counts without huge allocations. */
int main(void)
{
    List nodes[130], *owner;
    WORD output;
    int n,i;
    for(n=0;n<=129;n++){
        for(i=0;i<n;i++) nodes[i].tail=i+1<n ? nodes+i+1 : H;
        owner=n ? nodes : H; output=37; R.s=&owner; R.d=&output; raised=nil;
        if(setjmp(boundary)==0){
            lenl();
            if(n>127 || output!=n) abort();
        }else if(n<=127 || raised!=exBounds || output!=37) abort();
    }
    puts("PASS list length reduced counter: 130 limit/publication cases");
    return 0;
}
''')
    subprocess.run(command+[str(counter), '-o', str(td/'counter')], check=True)
    subprocess.run([str(td/'counter')], check=True, timeout=10)
