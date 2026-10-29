#!/usr/bin/env python3
"""Execute real emu/os debugger, coverage and timing loops with the production decoder."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Shared VM and platform device source tree.
KINDS = {'emu-debug': 'emu/port/devprog.c', 'os-debug': 'os/port/devprog.c',
         'emu-profile': 'emu/port/devprof.c', 'os-profile': 'os/port/devprof.c',
         'bench': 'os/port/devbench.c'}


# Extract complete bodies without interpreting braces inside C comments and strings.
def function(source, signature):
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
    raise RuntimeError(signature)


bench = (ROOT/'os/port/devbench.c').read_text()
debug = (ROOT/'emu/port/devprog.c').read_text()
profile = (ROOT/'emu/port/devprof.c').read_text()
declarations = debug[debug.index('typedef struct Bpt'):debug.index('#define\tQSHIFT')]
declarations += profile[profile.index('typedef struct Record'):profile.index('\nstruct\n{\n\tLock\tl;')]
# Current private type declarations and stat body are shared with parent replays;
# the changed loop/lookup/reset/name bodies themselves come from INSTRUMENT_SOURCE.
start = bench.index('typedef struct  {', bench.index('/* DIS operation timing */'))
declarations += bench[start:bench.index('extern struct', start)]
xec = (ROOT/'libinterp/xec.c').read_text()
parts = [function(xec, 'OP(addw)')]
selected = os.environ.get('INSTRUMENT_KIND')
if selected is not None and selected not in KINDS:
    raise ValueError(selected)
if os.environ.get('INSTRUMENT_SOURCE') and selected is None:
    raise ValueError('a parent source needs an explicit INSTRUMENT_KIND')
with tempfile.TemporaryDirectory(prefix='instrument-loops-') as temporary:
    work = Path(temporary)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    widths = tuple(map(int, os.environ.get('INSTRUMENT_WORD_BITS', '32 64').split()))
    if not widths or any(bits not in (32, 64) for bits in widths):
        raise ValueError('INSTRUMENT_WORD_BITS must select 32 and/or 64')
    for bits in widths:
        model = work/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp','lib9')]
        decoder = model/'decoder.o'
        subprocess.run(cc+flags+includes+['-Ddec=realdec','-c',str(ROOT/'libinterp/dec.c'),'-o',str(decoder)], check=True)
        for kind in ([selected] if selected else KINDS):
            source = Path(os.environ.get('INSTRUMENT_SOURCE', ROOT/KINDS[kind])).read_text()
            if kind.endswith('debug'):
                body = function(source, 'void\ndbgxec(')
                signature = 'dbgxec'
                define = '#define TEST_DEBUG 1 // Actual debugger body under test.\n'
            elif kind.endswith('profile'):
                body = function(source, 'static void\ncpxec(')
                signature = 'cpxec'
                define = '#define TEST_PROFILE 1 // Actual coverage body under test.\n'
            else:
                body = function(source, 'void\nbxec(')
                signature = 'bxec'
                define = '#define TEST_BENCH 1 // Actual timing and record ownership bodies under test.\n'
            lookupsource = source if kind == 'bench' else bench
            lookup = '\n\n\n'.join(function(lookupsource, sig if sig in lookupsource else sig.replace('char*\n', 'char *\n').replace('Mstat*\n', 'Mstat *\n')) for sig in
                                      ('static char*\nopname(', 'static void\nmreset(', 'static ulong\nhash(', 'static Mstat*\nmlookup('))
            compiled_start = bench.index('static struct {', bench.index('/* compiled code Dis timing */'))
            compiled_end = bench.index('/* Close only samples', compiled_start)
            lookup += '\n'+bench[compiled_start:compiled_end]
            lookup += '\n\n'.join(function(lookupsource, sig) for sig in ('void\ndopostcomp(', 'void\npostcomp(', 'void\nprecomp('))
            # Older definitions place a space before the pointer-return newline.
            unit = (ROOT/'libinterp/tests/instrument-loops.c').read_text()
            define += '#define INSTRUMENT_LABEL "'+kind+'" // Device executor selected for this binary.\n'
            unit = define+unit.replace('DECLARATIONS', declarations).replace('SCALARS', '\n\n'.join(parts))
            unit = unit.replace('BENCH_HELPERS', lookup).replace('LOOP_BODY', body).replace('EXECUTE', signature)
            unit = unit.replace('PROFILE_ARGS', ', int limbo, int vm, int scale, int origin' if kind.startswith('emu-') else ', int vm, int scale, int origin')
            target = model/(kind+'.c')
            target.write_text(unit)
            binary = model/kind
            formatter = [str(ROOT/'lib9'/(name+'.c')) for name in ('rune','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen')]
            subprocess.run(cc+flags+includes+[str(target),str(decoder)]+formatter+['-o',str(binary)], check=True)
            try:
                subprocess.run([str(binary)], check=True, timeout=60)
            except subprocess.CalledProcessError:
                if os.environ.get('INSTRUMENT_KEEP_ARTIFACT'):
                    shutil.copy2(binary, os.environ['INSTRUMENT_KEEP_ARTIFACT'])
                    shutil.copy2(target, os.environ['INSTRUMENT_KEEP_ARTIFACT']+'.c')
                raise
