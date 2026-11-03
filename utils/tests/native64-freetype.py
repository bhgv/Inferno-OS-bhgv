#!/usr/bin/env python3
"""Check both actual Freetype bridge layers with native target declarations and bounded renderer data."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract owning entry bodies; a control restores one old transform-layout assumption.
def units(control):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    extract = support['function']
    vm = (ROOT/'libinterp/freetype.c').read_text()
    native = (ROOT/'libfreetype/freetype.c').read_text()
    parts = [extract(vm, signature) for signature in
             ('static int\nftargument(', 'static Face*\nckface(', 'void\nFace_haschar(', 'void\nFace_loadglyph(', 'void\nFreetype_newface(', 'void\nFace_settransform(')]
    entries = '\n\n'.join(parts)
    bridge = extract(native, 'void\nftsettransform(')
    if control == 'vm-transform':
        start = entries.index('\tif(f->m != H){')
        end = entries.index('\tdestroy(*f->ret);', start)
        entries = entries[:start]+'''\tif(f->m != H) m = (FTmatrix*)f->m;
\tif(f->v != H) v = (FTvector*)f->v;
'''+entries[end:]
    elif control == 'result':
        old = 'present = fthaschar(face->ftface, c);\n\tacquire();\n\t*f->ret = present;'
        if entries.count(old) != 1:
            raise RuntimeError('Character result control drift')
        entries = entries.replace(old, '*f->ret = fthaschar(face->ftface, c);\n\tacquire();', 1)
    elif control == 'pitch':
        old = 'pitch < ftg.width'
        if entries.count(old) != 1:
            raise RuntimeError('Glyph pitch control drift')
        entries = entries.replace(old, 'pitch < 0', 1)
    elif control == 'native-transform':
        bridge = '''void
ftsettransform(FTface f, FTmatrix *m, FTvector *v)
{
    FT_Set_Transform(f.ft_face, (FT_Matrix*)m, (FT_Vector*)v);
}'''
    fixture = (ROOT/'utils/tests/native64-freetype-bridge.c').read_text()
    fixture = fixture.replace('FONT_WRAPPER', vm[vm.index('typedef struct Face Face;'):vm.index('Type*\tTMatrix')])
    fixture = fixture.replace('FONT_ENTRIES', entries)
    sorting = '#define KERN_INDEX(g1,g2) (((FT_ULong)(g1)<<16)|(g2)) // Retained native glyph-key encoding.\n'
    for file, name in (('t1afm.c', 'compare_kern_pairs'), ('psmodule.c', 'compare_uni_maps')):
        sorting += extract((ROOT/('libfreetype/'+file)).read_text(), 'FT_CALLBACK_DEF( int )\n  '+name+'(')+'\n\n'
    if control == 'sort':
        old = 'return index1 < index2 ? -1 : index1 > index2 ? 1 : 0;'
        if sorting.count(old) != 1:
            raise RuntimeError('Kerning sort control drift')
        sorting = sorting.replace(old, 'return index1-index2;', 1)
    native_fixture = (ROOT/'utils/tests/native64-freetype-native.c').read_text()
    native_fixture = native_fixture.replace('SORT_ENTRIES', sorting)
    native_fixture = native_fixture.replace('NATIVE_ENTRIES', bridge+'\n\n'+extract(native, 'char*\nftloadglyph('))
    return support['portfixture'](fixture, '#include "lib9.h"'), support['portfixture'](native_fixture, '#include "lib9.h"')


# Compile the retained engine, using its libc I/O adapter as an explicit standalone boundary.
def engine(work, compiler, flags, includes, runner, env):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    prelude = work/'transport.h'
    prelude.write_text(support['portfixture']('#include "lib9.h"', '#include "lib9.h"'))
    names = ('freetype', 'autohint', 'ftbase', 'ftglyph', 'cff', 'psaux', 'psnames',
             'raster', 'sfnt', 'smooth', 'truetype', 'type1', 'type42', 'ftsystem',
             'ftinit', 'pshinter', 'pfr')
    flags = flags+['-Werror=incompatible-pointer-types']
    objects = []
    sort = work/'qsort.o'
    subprocess.run(compiler+flags+includes+['-include', str(prelude), '-c', str(ROOT/'lib9/qsort.c'), '-o', str(sort)], env=env, check=True)
    objects.append(str(sort))
    for name in names:
        obj = work/(name+'.o')
        subprocess.run(compiler+flags+includes+['-include', str(prelude), '-c', str(ROOT/('libfreetype/'+name+'.c')), '-o', str(obj)], env=env, check=True)
        objects.append(str(obj))
    binary = work/'engine'
    subprocess.run(compiler+flags+includes+['-include', str(prelude), str(ROOT/'utils/tests/native64-freetype-engine.c')]+objects+['-lm', '-o', str(binary)], env=env, check=True)
    fonts = [str(ROOT/'fonts/ttf'/name) for name in ('Vera.ttf', 'noto/NotoSans-Regular.ttf')]
    subprocess.run(runner+[str(binary)]+fonts, env=env, check=True, timeout=60)


# Strict controls require the expected semantic refusal, never a sanitizer or compiler failure.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', action='store_true', help='Also render two bundled fonts with the actual vendored engine and libc I/O')
    parser.add_argument('--control', choices=('vm-transform', 'native-transform', 'pitch', 'limits', 'result', 'sort'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'include', 'libinterp', 'include/freetype', 'libfreetype')]
    vm, native = units(args.control)
    with tempfile.TemporaryDirectory(prefix='native64-freetype-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        if args.engine:
            if args.control:
                parser.error('--engine cannot be a semantic control')
            engine(work, compiler, flags, includes, runner, env)
        for label, source, models in (('vm', vm, (32, 64)), ('native', native, (32,))):
            if args.control and label != ('vm' if args.control in ('vm-transform', 'pitch', 'result') else 'native'):
                continue
            for bits in models:
                if args.control and label == 'vm' and bits != (64 if args.control == 'vm-transform' else 32):
                    continue
                if args.control == 'limits':
                    header = (ROOT/'include/freetype/config/ftstdlib.h').read_text()
                    old = '#include <limits.h>'
                    if header.count(old) != 1:
                        raise RuntimeError('Native limits control drift')
                    shadow = work/'freetype/config'
                    shadow.mkdir(parents=True, exist_ok=True)
                    (shadow/'ftstdlib.h').write_text(header.replace(old, '#define UINT_MAX 0xffffffffU // Restored legacy int limit.\n#define ULONG_MAX 0xffffffffUL // Restored legacy long limit.', 1))
                path = work/(label+str(bits)+'.c')
                binary = path.with_suffix('')
                path.write_text(source)
                subprocess.run(compiler+flags+(['-DDIS_WORD32=1'] if bits == 32 else [])+['-I'+str(work)]+includes+[str(path), '-o', str(binary)], env=env, check=True)
                result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=30)
                if args.control:
                    # Native records include adjacent bounded storage, so the old cast reaches
                    # a semantic predicate before any memory violation.
                    if result.returncode != 10 or 'Freetype '+label.replace('vm','bridge')+' mismatch' not in result.stderr:
                        raise RuntimeError('Freetype control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                    print('PASS Freetype control:', args.control, arch, flush=True)
                else:
                    if result.returncode:
                        raise RuntimeError(result.stdout+result.stderr)
                    print(result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
