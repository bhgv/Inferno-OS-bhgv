#!/usr/bin/env python3
"""Construct real runtime modules from fresh generated native64 records, including map8 >16 bytes."""
from pathlib import Path
import os
import argparse
import resource
import signal
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract complete owning bodies while ignoring braces inside comments and strings.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError('Unterminated production body: '+signature)


# Share the actual constructor closure and the bounded allocation/error transport with integration gates.
def constructor():
    heap = (ROOT/'libinterp/heap.c').read_text()
    load = (ROOT/'libinterp/load.c').read_text()
    link = (ROOT/'libinterp/link.c').read_text()
    runt = (ROOT/'libinterp/runt.c').read_text()
    parts = [function((ROOT/'emu/port/dis.c').read_text(), 'int\nkprotect(')]
    parts += [function(heap, s) for s in ('Type*\ndtype(', 'void\nfreetype(')]
    parts += [function(load, s) for s in ('Module*\nnewmod(', 'void\nfreemod(', 'void\nunload(')]
    parts += [function(link, s) for s in ('static void\nnewlink(', 'void\nruntime(', 'void\ndestroylinks(')]
    parts += [function((ROOT/'libinterp/dlm-Inferno.c').read_text(), 'void\nfreedyncode(')]
    parts += [runt[runt.index('typedef struct Native64Builtin Native64Builtin;'):runt.index('/* Prepare the string before publishing')]]
    adapter = (ROOT/'libinterp/tests/builtin-modules.c').read_text().split('/* Build a small ABI-real table')[0]
    adapter = adapter.replace('#include <assert.h>', '#define check(test) do { if(!(test)) { fprintf(stderr,"check failed at %d\\n",__LINE__); abort(); } } while(0) // Active test oracle.')
    adapter = adapter.replace('assert(', 'check(')
    adapter = adapter.replace('RUNTAB_TYPE', (ROOT/'libinterp/sysmod.h').read_text().splitlines()[0])
    adapter = adapter.replace(function(adapter, 'void\nkwerrstr('), """/* Preserve ordinary formatted diagnostic publication at the bounded process boundary. */
void
kwerrstr(char *message, ...)
{
    va_list arguments;

    va_start(arguments, message);
    vsnprintf(diagnostic, sizeof(diagnostic), message, arguments);
    va_end(arguments);
}


/* Read the same process diagnostic without requiring a complete native worker environment. */
void
kgerrstr(char *output, uint bytes)
{
    if(bytes) snprintf(output, bytes, "%s", diagnostic);
}""")
    adapter = adapter.replace('IMPLEMENTATION', '#include "native64-entry.h"\n#include "native64-bind.h"\n#include "native64-model.h"\n#include "native64-modules.h"\n'+'\n'.join(parts))
    return adapter


# Use the existing allocation/error boundary with active checks, never disabled NDEBUG assertions.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sets', action='store_true', help='Also check atomic module sets and all staged failures')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='native64-builtins-', dir=ROOT) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        if env.get('NATIVE64_LIMBO'):
            limbo = Path(env['NATIVE64_LIMBO']).resolve()
        else:
            build = dict(env, INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_LIMBO='1', MKBOOTSTRAP_BINDIR=str(work/'tools'))
            subprocess.run(['sh', str(ROOT/'makemk.sh')], cwd=work, env=build, check=True)
            limbo = work/'tools/limbo'
        arguments = ','.join('a'+str(i)+': string' for i in range(150))
        source = work/'probe.m'
        source.write_text('Probe: module { add: fn(x,y: int): int; many: fn('+arguments+'); };')
        header = subprocess.check_output([str(limbo), '-H', str(source)], cwd=work, env=env)
        (work/'builtin-native64.h').write_bytes(header)
        (work/'math-native64.h').write_bytes(subprocess.check_output([str(limbo), '-H', str(ROOT/'module/math.m')], cwd=work, env=env))
        adapter = constructor()
        unit = work/'test.c'
        body = (ROOT/'utils/tests/native64-builtins.c').read_text()
        if args.sets:
            adapter = adapter.replace('static void*\nallocate(size_t size)', 'static void batchobserve(void);\nstatic int batchdisturb; // Explicit cleanup diagnostic boundary.\n\n\n/* Bound native allocation while checking no staged module is visible. */\nstatic void*\nallocate(size_t size)')
            adapter = adapter.replace('\tcount = 0;\n', '\tbatchobserve();\n\tcount = 0;\n', 1)
            adapter = adapter.replace('\t\t\tlive--;\n', '\t\t\tlive--;\n\t\t\tif(batchdisturb) snprintf(diagnostic, sizeof(diagnostic), "%s", "cleanup changed error");\n', 1)
            body = body.replace('int\nmain(void)', 'int\nnative64_single_main(void)', 1)
            body += (ROOT/'utils/tests/native64-module-set.c').read_text()
        unit.write_text(adapter+body)
        compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/amd64/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1']), ('legacy-refused', ['-DNATIVE64_REFUSED']), ('packed-refused', ['-DDIS_WORD32=1', '-fpack-struct=1', '-DNATIVE64_REFUSED'])]:
            binary = work/label
            # Compile the complete owning runtime as well as the exercised constructor closure.
            subprocess.run(compiler+flags+model+includes+['-c', str(ROOT/'libinterp/runt.c'), '-o', str(work/(label+'.o'))], env=env, check=True)
            mathobj = work/(label+'-math.o')
            if label != 'legacy-refused':
                subprocess.run(compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections', '-DDIS_NATIVE64_MATH_HEADER=\"math-native64.h\"', '-c', str(ROOT/'libinterp/math.c'), '-o', str(mathobj)], env=env, check=True)
            objects = [str(mathobj)] if label != 'legacy-refused' else []
            subprocess.run(compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections', str(unit)]+objects+['-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
            subprocess.run([str(binary)], env=env, check=True)
            if args.sets and label in ('WORD32', 'WORD32-file'):
                for name, boundary, replacement in (
                        ('publication', 'build->staged[i] = build->current.module;', 'modules = build->current.module; build->staged[i] = build->current.module;'),
                        ('retirement', 'while(build->completed != 0)', 'while(0)')):
                    if (adapter+body).count(boundary) != 1:
                        raise RuntimeError('Module-set control boundary changed: '+name)
                    control = work/(label+'-'+name+'.c')
                    control.write_text((adapter+body).replace(boundary, replacement, 1))
                    target = work/(label+'-'+name)
                    subprocess.run(compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections', str(control)]+objects+['-Wl,--gc-sections', '-lm', '-o', str(target)], env=env, check=True)
                    result = subprocess.run([str(target)], env=env, capture_output=True, text=True)
                    if result.returncode != -signal.SIGABRT or 'check failed' not in result.stderr:
                        raise RuntimeError('Module-set removal control did not fail its predicate: '+name+'\n'+result.stdout+result.stderr)
                    print('PASS module-set removal control:', label, name, flush=True)
            print('PASS native64 builtin construction:', label, flush=True)


if __name__ == '__main__':
    main()
