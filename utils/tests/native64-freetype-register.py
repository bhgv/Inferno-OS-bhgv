#!/usr/bin/env python3
"""Register and invoke all Freetype exports with fresh selected native64 declarations and the real font engine."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Require deterministic compiler output and early CLI/dependency refusal before any header publication.
def declarations(limbo, work, env):
    source = work/'freetype-interface.m'
    source.write_text('include "sys.m";\ninclude "draw.m";\ninclude "freetype.m";\n')
    command = [str(limbo), '-H', '-M', 'Freetype', '-I'+str(ROOT/'module'), str(source)]
    first = subprocess.check_output(command, env=env)
    if first != subprocess.check_output(command, env=env):
        raise RuntimeError('Selected module header is nondeterministic')
    header = first.decode()
    for required in ('struct Draw_Point {', 'struct Freetype_Glyph {', 'limbo_native64_Freetype_spec_count 6'):
        if required not in header:
            raise RuntimeError('Missing selected/dependent declaration: '+required)
    for forbidden in ('F_Draw_', 'F_Sys_', 'limbo_native64_Draw_layout', 'Sys_Dir_size'):
        if forbidden in header:
            raise RuntimeError('Unselected module leaked into header: '+forbidden)
    for arguments, marker in (
        (['-M', 'Freetype', str(source)], 'usage:'),
        (['-H', '-M', '', str(source)], 'usage:'),
        (['-H', '-M'], 'usage:'),
        (['-H', '-M', 'Freetype', '-M', 'Freetype', str(source)], 'usage:'),
        (['-H', '-M', 'Missing', '-I'+str(ROOT/'module'), str(source)], 'selected module is missing'),
    ):
        result = subprocess.run([str(limbo)]+arguments, env=env, capture_output=True, text=True)
        if result.returncode == 0 or result.stdout or marker not in result.stderr:
            raise RuntimeError('Selected module CLI refusal missed: '+repr(arguments)+' '+result.stdout+result.stderr)
    dependency = work/'dependency.m'
    dependency.write_text('Bad: module { X: adt { pick { A => x: int; } }; }; Good: module { f: fn(x: int): int; };')
    good = subprocess.check_output([str(limbo), '-H', '-M', 'Good', str(dependency)], env=env)
    if b'Bad_' in good or b'limbo_native64_Good_layout' not in good:
        raise RuntimeError('Unselected unsupported declaration was emitted')
    dependency.write_text('Bad: module { X: adt { pick { A => x: int; } }; }; Good: module { f: fn(x: ref Bad->X): int; };')
    result = subprocess.run([str(limbo), '-H', '-M', 'Good', str(dependency)], env=env, capture_output=True, text=True)
    if result.returncode == 0 or result.stdout or 'picked types' not in result.stderr:
        raise RuntimeError('Selected dependency bypassed shape refusal: '+result.stdout+result.stderr)
    print('PASS selected native64 declarations: real Sys/Draw/Freetype input; dependency closure; 7 early refusals/selection cases', flush=True)
    return header


# Keep real constructor, Type maps, initmem/freeptrs and published destructor bodies.
def fixture():
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    extract = support['function']
    body = support['constructor']()
    for signature in ('void\nfreeheap(', 'void\ndestroy('):
        body = body.replace(extract(body, signature), '', 1)
    body = body.replace('static int live,', 'static void fontobserve(void);\nstatic int live,', 1)
    body = body.replace('\tcount = 0;\n', '\tfontobserve();\n\tcount = 0;\n', 1)
    heap = (ROOT/'libinterp/heap.c').read_text()
    body += '\n\n'+'\n\n'.join(extract(heap, signature) for signature in
                                    ('void\ninitmem(', 'void\nfreeptrs(', 'void\nfreeheap('))
    return body+'\n\n'+(ROOT/'utils/tests/native64-freetype-register.c').read_text()


# Compile the full owning consumer with only path allocation routed through the bounded registry.
def consumer(control):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    body = (ROOT/'libinterp/freetype.c').read_text()
    body = support['portfixture'](body, '#include <lib9.h>')
    boundary = '#include "freetype.h"'
    if body.count(boundary) != 1:
        raise RuntimeError('Freetype consumer transport boundary drift')
    body = body.replace(boundary, boundary+'''
extern char *fontduplicate(char*); // Bounded native path owner in this fixture only.
extern void fontfree(void*); // Matching path retirement boundary.
#define strdup fontduplicate // Route path duplication after all real declarations.
#define free fontfree // Do not rename descriptor fields while parsing headers.
''', 1)
    if control == 'retire':
        old = '\t\tfreetype(types[i]);'
        if body.count(old) != 1:
            raise RuntimeError('Freetype creator retirement boundary drift')
        body = body.replace(old, '\t\tUSED(types[i]);', 1)
    return body


# Exercise real registration and all exports, then require three semantic mutation controls.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    with tempfile.TemporaryDirectory(prefix='native64-font-register-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        header = declarations(args.limbo.resolve(), work, env)
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in
                   ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp', 'include/freetype', 'libfreetype')]
        (work/'freetype-native64.h').write_text(header)
        negative = work/'negative.c'
        negative.write_text(consumer(None))
        selected = '-DDIS_NATIVE64_FREETYPE_HEADER="freetype-native64.h"'
        for options, message in (
            (['-DDIS_WORD32=1', selected, '-DDIS_NATIVE64_FREETYPE_REGISTER=0'], 'must be 1'),
            (['-DDIS_WORD32=1', selected, '-DDIS_NATIVE64_FREETYPE_REGISTER=2'], 'must be 1'),
            (['-DDIS_WORD32=1', '-DDIS_NATIVE64_FREETYPE_REGISTER=1'], 'requires an explicit generated header'),
            ([selected, '-DDIS_NATIVE64_FREETYPE_REGISTER=1'], 'require DIS_WORD32=1'),
            (['-DDIS_WORD32=0', selected, '-DDIS_NATIVE64_FREETYPE_REGISTER=1'], 'require DIS_WORD32=1'),
        ):
            refused = subprocess.run(compiler+flags+includes+options+['-fsyntax-only', str(negative)], env=env, capture_output=True, text=True)
            if refused.returncode == 0 or message not in refused.stderr:
                raise RuntimeError('Native Freetype configuration guard missed: '+repr(options)+' '+refused.stderr)
        print('PASS native64 Freetype configuration: 5 rejected build selections', flush=True)
        engine = runpy.run_path(str(ROOT/'utils/tests/native64-freetype.py'))['engine']
        objects = engine(work, compiler, flags, includes, runner, env)
        unit = work/'registration.c'
        unit.write_text(fixture())
        source = work/'consumer.c'
        fonts = [str(ROOT/'fonts/ttf'/name) for name in ('Vera.ttf', 'noto/NotoSans-Regular.ttf')]
        for mode in ('WORD32', 'WORD32-file', 'header-only', 'packed', 'map', 'frame', 'retire'):
            selected = header
            if mode in ('map', 'frame'):
                old, new = {
                    'map': ('#define F_Freetype_Face_haschar_map {0x00,0x80}', '#define F_Freetype_Face_haschar_map {0x00,0x00}'),
                    'frame': ('#define F_Freetype_Face_haschar_size 80', '#define F_Freetype_Face_haschar_size 88'),
                }[mode]
                if selected.count(old) != 1:
                    raise RuntimeError('Generated control boundary drift: '+mode)
                selected = selected.replace(old, new, 1)
            (work/'freetype-native64.h').write_text(selected)
            source.write_text(consumer(mode))
            model = ['-DDIS_WORD32=1']
            if mode == 'WORD32-file':
                model += ['-DDIS64_PILOT_LOADER=1']
            if mode == 'header-only':
                model += ['-DFONT_HEADER_ONLY=1']
            if mode == 'packed':
                model += ['-fpack-struct=1', '-DFONT_PACKED=1']
            command = compiler+flags+model+includes
            obj = work/('consumer-'+mode+'.o')
            selection = ['-DDIS_NATIVE64_FREETYPE_HEADER="freetype-native64.h"']
            if mode != 'header-only':
                selection += ['-DDIS_NATIVE64_FREETYPE_REGISTER=1']
            subprocess.run(command+selection+['-c', str(source), '-o', str(obj)], env=env, check=True)
            binary = work/mode
            subprocess.run(command+[str(unit), str(obj)]+objects+['-lm', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)]+fonts, env=env, capture_output=True, text=True, timeout=60)
            if mode in ('map', 'frame', 'retire'):
                if result.returncode != 10 or 'Freetype registration mismatch' not in result.stderr:
                    raise RuntimeError('Registration control missed semantic predicate: '+mode+' '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS native64 Freetype registration control:', mode, arch, flush=True)
            else:
                if result.returncode:
                    raise RuntimeError(mode+' '+result.stdout+result.stderr)
                print(mode+': '+result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
