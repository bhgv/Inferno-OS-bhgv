#!/usr/bin/env python3
"""Compare real scalar channel allocation with production AMD64 RX constructors."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract production functions while ignoring braces inside comments and literals.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


xec = Path(os.environ.get('CHANNEL_XEC_SOURCE', ROOT/'libinterp/xec.c')).read_text()
if os.environ.get('CHANNEL_TYPE_SOURCE'):
    baseline = Path(os.environ['CHANNEL_TYPE_SOURCE']).read_text()
    signature = 'Channel*\ncnewc('
    xec = xec.replace(function(xec,signature),function(baseline,signature))
if os.environ.get('CHANNEL_ARGUMENT_SOURCE'):
    baseline = Path(os.environ['CHANNEL_ARGUMENT_SOURCE']).read_text()
    for signature in ('Channel*\nnewc(', 'OP(newcmp)'):
        xec = xec.replace(function(xec,signature),function(baseline,signature))
heap = (ROOT/'libinterp/heap.c').read_text()
dis = (ROOT/'emu/port/dis.c').read_text()
parts = [function(dis, 'int\nkprotect('), function(dis, 'void\nkrethrow(')]
parts += [function(heap, result+'\n'+name+'(') for name, result in
          [('dtype','Type*'), ('freetype','void'), ('acheck','void'), ('initmem','void'),
           ('freeptrs','void'), ('nheap','Heap*'), ('heap','Heap*'), ('heaparray','Heap*'),
           ('initarray','void'), ('freechan','void'), ('freearray','void'), ('freeheap','void')]]
parts += [xec[xec.index('typedef struct ChannelBuild'):xec.index('/* Select builtin scalar payloads')]]
parts += [function(xec, 'Channel*\ncnewcscalar('), function(xec, 'static Type*\nmoduletype('), function(xec, 'OP(newcmp)'), function(xec, 'Channel*\ncnewcmapped(')]
parts += [xec[xec.index('typedef struct RawChannel'):xec.index('/* Create a mapped channel')]]
if os.environ.get('CHANNEL_RAW_SOURCE'):
    baseline = Path(os.environ['CHANNEL_RAW_SOURCE']).read_text()
    start = baseline.index('typedef struct RawChannel')
    stop = baseline.index('OP(newcm)',start)
    legacy = baseline[start:stop].replace('RawChannel','LegacyRawChannel').replace('rawchannel','legacyrawchannel')
    body = function(baseline,'OP(newcm)').replace('RawChannel','LegacyRawChannel').replace('rawchannel','legacyrawchannel')
    current = function(xec,'OP(newcm)')
    parts = [legacy+part.replace(current,body) if current in part else part for part in parts]
# Reuse only the existing bounded allocator/queue model, never its legacy test main.
preamble = (ROOT/'libinterp/tests/channels.c').read_text().split('IMPLEMENTATION')[0]
preamble = preamble.replace('#include <assert.h>', '#include "comp-amd64.c"')
preamble = preamble.replace('#include "comp-amd64.c"', '#include "comp-amd64.c"\n#include "tests/leaf-mapped-cons-stub.h"')
for header in ('isa.h','interp.h','raise.h','kernel.h'):
    preamble = preamble.replace('#include \"'+header+'\"\n', '')
preamble = preamble.replace('static Proc worker;', '''

/* Fail explicitly so NDEBUG cannot disable fixture invariants. */
static void
testcheck(int condition, int line)
{
    if(!condition){
        fprintf(stderr,"channel check failed at %d\\n",line);
        abort();
    }
}
#define check(condition) testcheck((condition), __LINE__) // Keep explicit checks with a source location.
static Proc worker;''').replace('assert(', 'check(').replace('check(p);', 'check(p != nil);')
preamble = preamble.replace('#define malloc(n)', '''

/* Keep executable-table metadata outside the measured VM resource allocator. */
void*
mallocz(ulong size, int clear)
{
    void *p=malloc(size);
    if(p != nil && clear)memset(p,0,size);
    return p;
}
#define malloc(n)''')
preamble += '#include <fenv.h>\n#include <stddef.h>\n'
preamble += '''
Type Tbyte={1,0,0,1}, Tword={1,0,0,sizeof(WORD)}, Tlong={1,0,0,8};
Type Treal={1,0,0,8}, Tptr={1,0,0,sizeof(void*),1,0,0,{0x80}}, Tstring={1};
char exInval[]="invalid",exModule[]="module";


/* Scalar movers identify buffer types; queue transfer has its own runtime suite. */
void
movb(void)
{
}


/* Identify the WORD mover without replacing queue-transfer regressions. */
void
movw(void)
{
}


/* Identify the big-integer mover for real channel construction. */
void
movl(void)
{
}


/* Identify the real mover without performing a queued transfer. */
void
movf(void)
{
}


/* Identify the pointer mover while real buffer cleanup checks its map. */
void
movp(void)
{
}


/* This fixture never executes list allocation. */
List*
cons(ulong size, void *slot)
{
    (void)size;
    (void)slot;
    abort();
    return H;
}
'''
if '--payload' in sys.argv:
    preamble = preamble.replace('void destroy(void *p) {', r'''
static void *publication_old; // Old owner whose release observes the newly published channel.
static WORD publication_width; // Captured raw width required before any old-owner callback.
static int publication_watch; // Enable one publication-order oracle during replacement.


/* Inspect the new raw metadata at the exact old-owner release boundary. */
static void
checkpublication(void *object)
{
    Channel *published;

    if(publication_watch && object==publication_old){
        memmove(&published,R.d,sizeof(published));
        check(published!=object && published!=H && published->mid.w==publication_width);
        publication_watch=0;
    }
}


/* Release bounded fixture owners after checking raw replacement ordering. */
void destroy(void *p) {''')
    preamble = preamble.replace('if(p==H)return;', 'checkpublication(p);\n    if(p==H)return;')
if '--captured' in sys.argv:
    preamble = '#define TEST_LEAF_DISPATCH_RUNTIME // Exercise real complete dispatch.\n'+preamble
    preamble += '\nvoid (*optab[256])(void);\nchar exOp[]="opcode";\n'
    parts.append(function(xec, 'int\ncjitexec('))
fixture = ('channel-capture-native.c' if '--captured' in sys.argv else
           'payload-channel-native.c' if '--payload' in sys.argv else 'scalar-channel-native.c')
unit = preamble + '\n'.join(parts) + (ROOT/'libinterp/tests'/fixture).read_text()
if os.environ.get('CHANNEL_UNIT_OUT'):
    Path(os.environ['CHANNEL_UNIT_OUT']).write_text(unit)
with tempfile.TemporaryDirectory(prefix='scalar-channel-native-') as directory:
    td = Path(directory)
    target = td/'channels.c'
    target.write_text(unit)
    command = shlex.split(os.environ.get('CC','cc'))
    command += shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (4,8):
        model = td/str(wb)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb == 4:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        binary = td/('channels'+str(wb))
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        subprocess.run(command+includes+[str(target),'-o',str(binary),'-lm'],check=True)
        subprocess.run([str(binary)],check=True)
