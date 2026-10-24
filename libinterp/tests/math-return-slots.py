#!/usr/bin/env python3
"""Check real Math scalar bindings with unaligned and aliased VM return slots."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('MATH_RETURN_SOURCE', ROOT/'libinterp/math.c')).read_text()
frames = (ROOT/'libinterp/runt.h').read_text()


def function(signature):
    """Extract production bodies without counting braces in comments or literals."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


names = []
declarations = []
checks = []
for match in re.finditer(r'void\nMath_(\w+)\(void \*fp\)\n\{(.*?)\n}', source, re.S):
    name, body = match.groups()
    if not ('*f->ret =' in body or 'memmove(f->ret, &result,' in body):
        continue
    frame = re.search(r'struct F_Math_'+name+r'\n\{(.*?)\n\};', frames, re.S).group(1)
    if 'Array*' in frame or 'Array *' in frame:
        continue
    typ = re.search(r'\b(REAL|WORD|LONG)\*\s*ret;', frame).group(1)
    declarations.append('typedef struct F_Math_'+name+' {'+frame+'\n} F_Math_'+name+';')
    fields = re.findall(r'\b(REAL|WORD|LONG)\s+(\w+);', frame)
    assignments = ''.join('storage.value.'+field+' = '+('input' if kind == 'REAL' else 'seed-2')+';' for kind, field in fields)
    # Compare aligned execution against byte-offset returns, then returns overlapping inputs.
    aliases = ''.join('test_'+name+'('+str(index)+');' for index in range(len(fields)+1))
    targets = ['memory.bytes+16+offset']+['(uchar*)&storage.value.'+field for _, field in fields]
    checks.append('''/* Compare the actual binding with its aligned execution and preserve guards. */
static void test_'''+name+'''(int alias)
{
    struct { F_Math_'''+name+''' value; uchar padding[16]; } storage;
    '''+typ+''' wanted;
    union { double alignment; uchar bytes[80]; } memory;
    uchar before[80], *destination;
    int offset, seed, mode, flags;
    double input;
    for(mode=0; mode<4; mode++)for(seed=0; seed<6; seed++)for(offset=0; offset<8; offset++){
        input=seed==0 ? -0.0 : seed==1 ? 1.25 : seed==2 ? -2.5 : seed==3 ? INFINITY : seed==4 ? NAN : 0x1p-1074;
        memset(&storage,0,sizeof(storage));'''+assignments+'''
        storage.value.ret=&wanted;assert(fesetround(modes[mode])==0);feclearexcept(FE_ALL_EXCEPT);
        Math_'''+name+'''(&storage.value);flags=fetestexcept(FE_ALL_EXCEPT);
        memset(memory.bytes,0xa5,sizeof(memory.bytes));memmove(before,memory.bytes,sizeof(before));
        destination=alias==0 ? '''+targets[0]+''.join(' : alias=='+str(i)+' ? '+v for i,v in enumerate(targets[1:],1))+''' : memory.bytes;
        storage.value.ret=(void*)destination;feclearexcept(FE_ALL_EXCEPT);
        Math_'''+name+'''(&storage.value);
        assert(!memcmp(destination,&wanted,sizeof(wanted)));
        assert(fetestexcept(FE_ALL_EXCEPT)==flags && fegetround()==modes[mode]);
        if(alias==0){memmove(before+16+offset,&wanted,sizeof(wanted));assert(!memcmp(before,memory.bytes,sizeof(before)));}
        checks++;
    }
}
''')
    names.append((name, aliases))

# Tuple bindings preserve generated-layout padding as well as both values.
for name in ('lgamma', 'modf'):
    frame = re.search(r'struct F_Math_'+name+r'\n\{(.*?)\n\};', frames, re.S).group(1)
    declarations.append('typedef struct F_Math_'+name+' {'+frame+'\n} F_Math_'+name+';')
    checks.append('''/* Compare unaligned tuple returns, their padding and input overlap. */
static void test_'''+name+'''(int alias)
{
    struct { F_Math_'''+name+''' value; uchar padding[32]; } storage;
    union { double align; uchar bytes[64]; } memory, wanted;
    uchar before[64], *destination;
    int offset, seed, mode, flags;
    for(mode=0;mode<4;mode++)for(seed=0;seed<3;seed++)for(offset=0;offset<8;offset++){
        memset(&storage,0xa5,sizeof(storage));memset(wanted.bytes,0xa5,sizeof(wanted.bytes));
        storage.value.x=seed==0 ? -0.0 : seed==1 ? 1.25 : -2.5;
        storage.value.ret=(void*)wanted.bytes;
        assert(fesetround(modes[mode])==0);feclearexcept(FE_ALL_EXCEPT);
        Math_'''+name+'''(&storage.value);flags=fetestexcept(FE_ALL_EXCEPT);
        memset(memory.bytes,0xa5,sizeof(memory.bytes));memmove(before,memory.bytes,sizeof(before));
        destination=alias ? (uchar*)&storage.value.x : memory.bytes+8+offset;
        /* The aliased tuple's initial padding comes from the original input bytes. */
        if(alias){
            memmove(wanted.bytes,destination,sizeof(*storage.value.ret));
            storage.value.ret=(void*)wanted.bytes;Math_'''+name+'''(&storage.value);
        }
        storage.value.ret=(void*)destination;feclearexcept(FE_ALL_EXCEPT);
        Math_'''+name+'''(&storage.value);
        assert(!memcmp(destination,wanted.bytes,sizeof(*storage.value.ret)));
        assert(fetestexcept(FE_ALL_EXCEPT)==flags && fegetround()==modes[mode]);
        if(!alias)memmove(before+8+offset,wanted.bytes,sizeof(*storage.value.ret));
        assert(!memcmp(memory.bytes,before,sizeof(before)));checks++;
    }
}
''')
    names.append((name, 'test_'+name+'(0);test_'+name+'(1);'))

prefix = r'''
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <fenv.h>
#include "isa.h"
typedef unsigned char uchar;
typedef double REAL;
typedef int64_t LONG;
typedef uint64_t uvlong;
#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
static unsigned long checks;
static int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
static char exInval[]="invalid";
/* Unexpected VM exceptions must fail this bounded valid-input matrix. */
static void error(char *s){(void)s;abort();}
/* Observe result-slot handling separately from platform FP-control implementations. */
static int getFPcontrol(void){return 0x1357;}
static int getFPstatus(void){return 0x2468;}
static int FPcontrol(WORD a,WORD b){return a^b;}
static int FPstatus(WORD a,WORD b){return a^b;}
/* Supply the native scalar API used by the production bindings. */
static double ipow10(int n){return pow(10.0,n);}
#define isNaN isnan
/* No allocations need leak-process inspection in this fixture. */
int __lsan_is_turned_off(void){return 1;}
'''
unions = source[source.index('typedef union MathBits64'):source.index('/* Check full VM lengths')]
helpers = '\n'.join(function(sig) for sig in ('static WORD\nmathsigned32(', 'static int\nmathorder(', 'static int\nmathexponent('))
implementation = '\n'.join(function('void\nMath_'+name+'(') for name, _ in names)
# Bind fdlibm entry points to host libm: this tests the production VM bindings,
# not the numerical implementation inside fdlibm.
implementation = re.sub(r'\b__ieee754_(\w+)', r'\1', implementation)
unit = prefix+unions+'\n'.join(declarations)+helpers+implementation+'\n'.join(checks)
unit += '\n/* Execute every scalar binding and restore the caller FP environment. */\nint main(void){fenv_t saved;assert(fegetenv(&saved)==0);'+''.join(calls for _, calls in names)+'assert(fesetenv(&saved)==0);printf("PASS Math return slots: %lu cases, '+str(len(names))+' bindings WORD%d\\n",checks,WORD_BITS);return 0;}\n'
with tempfile.TemporaryDirectory(prefix='math-return-') as directory:
    td = Path(directory)
    target = td/'returns.c'
    target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for bits in (32, 64):
        binary = td/('returns'+str(bits))
        subprocess.run(command+['-DWORD_BITS='+str(bits), '-I'+str(ROOT/'include'), str(target), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
