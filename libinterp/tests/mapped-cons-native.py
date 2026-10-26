#!/usr/bin/env python3
"""Check mapped list rollback and real AMD64 RX publication with native VM layouts."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Extract complete C functions while ignoring literal/comment braces.
def function(source, signature):
    start=source.index(signature)
    brace=source.index('{',start)
    depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':
            depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


xec=Path(os.environ.get('MAPPED_CONS_XEC',ROOT/'libinterp/xec.c')).read_text()
heap=(ROOT/'libinterp/heap.c').read_text()
dis=(ROOT/'emu/port/dis.c').read_text()
parts=[function(dis,'int\nkprotect('),function(dis,'void\nkrethrow(')]
parts += [function(heap,'void\n'+name+'(') for name in ('initmem','freeptrs','incmem')]
parts += [function(xec,sig) for sig in ('List*\ncons(','static Type*\nmoduletype(','Type*\ncconsmappedtype(')]
parts += [xec[xec.index('typedef struct ConsMapped'):xec.index('/* Preserve the destination, tail')]]
body=function(xec,'OP(consmp)')
if os.environ.get('MAPPED_CONS_PARENT'):
    body=function(Path(os.environ['MAPPED_CONS_PARENT']).read_text(),'OP(consmp)')
parts.append(body)
preamble=(ROOT/'libinterp/tests/lists.c').read_text().split('IMPLEMENTATION')[0]
preamble=preamble.replace('#include "isa.h"\n','').replace('#include "interp.h"\n','')
preamble=preamble.replace('#include "raise.h"', '#include "comp-amd64.c"\n#define TEST_REAL_MAPPED_CONS // Link the production descriptor validator.\n#include "tests/leaf-plain-stubs.h"')
preamble=preamble.replace('#include <assert.h>','''#include <fenv.h>


/* Preserve fixture checks under NDEBUG and report the production-unit location. */
static void
testcheck(int condition, int line)
{
    if(!condition){
        fprintf(stderr,"mapped cons check failed at %d\\n",line);
        abort();
    }
}
#define check(condition) testcheck((condition),__LINE__) // Explicit runtime test oracle.
''')
# stdio is required by the diagnostic helper above.
preamble='#include <stdio.h>\n'+preamble
preamble=preamble.replace('assert(', 'check(').replace('check(h);','check(h!=nil);')
preamble += '''
Type Tstring={1}, Tarray={1};


/* Keep executable publication metadata outside the VM allocation ledger. */
void*
mallocz(ulong size, int clear)
{
    void *object=malloc(size);
    if(object!=nil && clear){
        memset(object,0,size);
    }
    return object;
}
'''
unit=preamble+'\n\n'.join(parts)+(ROOT/'libinterp/tests/mapped-cons-native.c').read_text()
if os.environ.get('MAPPED_UNIT_OUT'):
    Path(os.environ['MAPPED_UNIT_OUT']).write_text(unit)
with tempfile.TemporaryDirectory(prefix='mapped-cons-native-') as directory:
    td=Path(directory)
    target=td/'cons.c'
    target.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))
    command += shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (4,8):
        model=td/str(wb)
        model.mkdir()
        header=(ROOT/'include/interp.h').read_text()
        if wb==4:
            header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
            header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
        (model/'interp.h').write_text(header)
        includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary=td/('cons'+str(wb))
        subprocess.run(command+includes+[str(target),'-lm','-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
