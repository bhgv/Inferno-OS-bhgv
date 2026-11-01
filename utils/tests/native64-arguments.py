#!/usr/bin/env python3
"""Build and retire actual managed typed-argument contexts through both protected port boundaries."""
from pathlib import Path
import argparse
import os
import resource
import runpy
import shlex
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Bound only native allocation/process services; retain real heap, collector, Type and error statements.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--formatter', action='store_true', help='Also exercise the actual shared formatter with typed slots and real lib9')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-result.py'))
    function = support['function']
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='native64-arguments-', dir=ROOT) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        limbo = args.limbo.resolve()
        roots = work/'roots.m'
        roots.write_text('Roots: module { Node: adt { next: ref Node; }; copy: fn(source: array of ref Node): array of ref Node; caller: fn(padding: big, result: array of byte, tail: array of byte); many: fn('+','.join('a'+str(i)+': array of byte' for i in range(150))+'); };')
        for name, source in [('math', ROOT/'module/math.m'), ('sys', ROOT/'module/sys.m'), ('roots', roots)]:
            (work/(name+'-native64.h')).write_bytes(subprocess.check_output([str(limbo), '-H', str(source)], cwd=work, env=env))
        compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])]:
            command = compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections']
            objects = []
            for name in ('heap', 'math'):
                obj = work/(label+'-'+name+'.o')
                options = ['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"'] if name == 'math' else []
                subprocess.run(command+options+['-c', str(ROOT/('libinterp/'+name+'.c')), '-o', str(obj)], env=env, check=True)
                objects.append(str(obj))
            for port in ('emu', 'os'):
                source = support['unit'](port).replace('#define SlotBytes 256', '#define SlotBytes 2048')
                source = source.replace('int\nmain(void)', 'int\nnative64_original_result_main(void)', 1)
                source = source.replace(function(source, 'void*\nmallocz('), 'void *mallocz(ulong, int);', 1)
                oldpool = function(source, 'void*\npoolalloc(')
                newpool = oldpool.replace('\tcheck(pool == heapmem', '\tif(contextheapfail) return nil;\n\tcheck(pool == heapmem', 1)
                source = source.replace(oldpool, 'static int contextheapfail; // Fail only the actual heapz pool boundary.\n\n'+newpool, 1)
                unit = work/(port+'.c')
                source += (ROOT/'utils/tests/native64-arguments.c').read_text()
                extra = []
                if args.formatter:
                    runtime = (ROOT/'libinterp/runt.c').read_text()
                    a = runtime.index('typedef struct XprintString')
                    b = runtime.index('/* Validate borrowed string', a)
                    core = runtime[a:b]
                    a = runtime.index('/* Private reader categories')
                    b = runtime.index('/* Preserve packed legacy strides', a)
                    core += runtime[a:b]
                    for signature in ('static int\nxsnprint(', 'static void\nxprintstring(', 'static uchar*\nxprintargument(', 'static void\nxprinttake(', 'static int\nxprintwithargs('):
                        core += '\n'+function(runtime, signature)+'\n'
                    strings = (ROOT/'libinterp/string.c').read_text()
                    core += '\n'+function(strings, 'String*\nnewstring(')+'\n'
                    core += '\n'+function(strings, 'String*\nnewrunes(')+'\n'
                    core += '\n'+function(strings, 'String*\nc2string(')+'\n'
                    core += '\n'+function(runtime, 'Array*\nmem2array(')+'\n'
                    source = source.replace('int\nmain(int argc, char **argv)', 'int\nnative64_arguments_main(int argc, char **argv)', 1)
                    source += '\nchar exInval[]="invalid", exBounds[]="bounds"; // Actual formatter error categories.\n'
                    source += 'char *syserr(char*, char*, Prog*);\nint hmsize(void*);\n'+core
                    source += (ROOT/'utils/tests/native64-format.c').read_text().replace('int\nmain(int argc, char **argv)', 'int\nnative64_formatter_main(int argc, char **argv)', 1)
                    source += (ROOT/'utils/tests/native64-sys-format.c').read_text()
                    for signature in ('int\nSys_sprint64(', 'int\nSys_aprint64('):
                        source += '\n'+function(runtime, signature)+'\n'
                    extra = [str(ROOT/('lib9/'+name+'.c')) for name in ('rune','sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+['-pthread']
                unit.write_text(source)
                binary = work/(label+'-'+port)
                subprocess.run(command+[str(unit)]+objects+extra+['-Wl,--wrap=free', '-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
                subprocess.run([str(binary)], env=env, check=True)
                for control in ('frame', 'types'):
                    result = subprocess.run([str(binary), control], env=env, capture_output=True, text=True)
                    if result.returncode != -signal.SIGABRT or 'GC root predicate failed' not in result.stderr:
                        raise RuntimeError('Argument ownership control did not reach its predicate: '+control+'\n'+result.stdout+result.stderr)
                if args.formatter:
                    for name, header, boundary, replacement in (('kind', 'native64-format.h', 'if(!valid)', 'if(0)'),
                            ('output', 'native64-format.h', 'native64formattedclear(&build.output);', '(void)0;'),
                            ('sys-result', 'native64-sys-format.h', 'if(!native64sysformatreference(nil, old, call->array ? &Tarray : &Tstring))', 'if(0)'),
                            ('sys-element', 'native64-sys-format.h', 'if(call->array && old != H && ((Array*)old)->t != &Tbyte)', 'if(0)')):
                        adapter = (ROOT/('include/'+header)).read_text()
                        if adapter.count(boundary) != 1:
                            raise RuntimeError('Typed format control boundary changed: '+name)
                        control_unit = work/(port+'-'+name+'-unchecked.c')
                        control_unit.write_text(source.replace('#include "'+header+'"', adapter.replace(boundary, replacement), 1))
                        control_binary = work/(label+'-'+port+'-'+name+'-unchecked')
                        subprocess.run(command+[str(control_unit)]+objects+extra+['-Wl,--wrap=free', '-Wl,--gc-sections', '-lm', '-o', str(control_binary)], env=env, check=True)
                        result = subprocess.run([str(control_binary)], env=env, capture_output=True, text=True)
                        if result.returncode != -signal.SIGABRT or 'GC root predicate failed' not in result.stderr:
                            raise RuntimeError('Typed format control did not reach its predicate: '+name+'\n'+result.stdout+result.stderr)
                        print('PASS typed formatter removal control:', name, port, label, flush=True)
                print('PASS managed argument contexts:', port, label, flush=True)


if __name__ == '__main__':
    main()
