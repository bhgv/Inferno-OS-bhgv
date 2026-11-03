#!/usr/bin/env python3
"""Exercise actual Prefab/Freetype descriptor initialization and native public maps."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Select real declarations and explicit managed-field expectations without executing graphics APIs.
def configuration(module, source):
    if module == 'prefab':
        header = '#include "prefab.h"'
        declarations = source[source.index('uchar elementmap[]'):source.index('/* Infrared remote')]
        callback = '''/* Observe the real callback identity; graphical object destruction is separate. */
void
freeprefabcompound(Heap *heap, int swept)
{
    USED(heap); USED(swept);
}
'''
        shapes = '''static GraphicsShape shapes[] = {
    {&TElement, sizeof(PElement), freeheap, elementmap, sizeof(elementmap),
      {offsetof(PElement,e.environ), offsetof(PElement,e.tag), offsetof(PElement,e.kids), offsetof(PElement,e.str), offsetof(PElement,e.mask), offsetof(PElement,e.image), offsetof(PElement,e.font)}, 7},
    {&TLayout, sizeof(Prefab_Layout), freeheap, layoutmap, sizeof(layoutmap),
      {offsetof(Prefab_Layout,font), offsetof(Prefab_Layout,color), offsetof(Prefab_Layout,text), offsetof(Prefab_Layout,icon), offsetof(Prefab_Layout,mask), offsetof(Prefab_Layout,tag)}, 6},
    {&TCompound, sizeof(PCompound), freeprefabcompound, compoundmap, sizeof(compoundmap),
      {offsetof(PCompound,c.image), offsetof(PCompound,c.environ), offsetof(PCompound,c.title), offsetof(PCompound,c.contents)}, 4}
};'''
        name, first = 'Prefab', 'TElement'
    else:
        header = '#include "freetype.h"'
        declarations = source[source.index('typedef struct Face Face;'):source.index('/* Build pointer maps')]
        callback = '''/* Keep the face descriptor identity without initializing a native FT library. */
static void
freeface(Heap *heap, int swept)
{
    USED(heap); USED(swept);
}
'''
        shapes = '''static GraphicsShape shapes[] = {
    {&TMatrix, sizeof(Freetype_Matrix), freeheap, Matrixmap, sizeof(Matrixmap), {0}, 0},
    {&TVector, sizeof(Freetype_Vector), freeheap, Vectormap, sizeof(Vectormap), {0}, 0},
    {&TFace, sizeof(Face), freeface, Facemap, sizeof(Facemap),
      {offsetof(Face,freetypeface.familyname), offsetof(Face,freetypeface.stylename)}, 2},
    {&TGlyph, sizeof(Freetype_Glyph), freeheap, Glyphmap, sizeof(Glyphmap),
      {offsetof(Freetype_Glyph,bitmap)}, 1}
};'''
        name, first = 'Freetype', 'TMatrix'
    declarations += '\n#define GUI_NAME "'+name+'" // Selected owning module identity.\n'
    declarations += '#define GUI_INITIALIZE '+module+'modinit // Actual production initializer.\n'
    declarations += 'static Runtab '+name+'modtab[1]; // Registration boundary only.\n'
    declarations += '#define '+name+'modlen 0 // No guest exports enabled by this fixture.\n'
    declarations += '#define GUI_TABLE '+name+'modtab // Observe the selected registration table.\n'
    declarations += '#define GUI_LENGTH '+name+'modlen // Expected boundary extent.\n'
    return header, declarations, callback, shapes, name, first


# Use actual dtype/freetype/kprotect while observing publication at every allocation boundary.
def unit(module, control):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    adapter = support['constructor']()
    adapter = adapter.replace('static int live,', 'static int disturb; // Cleanup diagnostic interference.\nstatic void graphicsobserve(void);\nstatic int live,', 1)
    adapter = adapter.replace('\tcount = 0;\n', '\tgraphicsobserve();\n\tcount = 0;\n', 1)
    adapter = adapter.replace('\t\t\tlive--;\n', '\t\t\tlive--;\n\t\t\tif(disturb) strcpy(diagnostic, "cleanup diagnostic");\n', 1)
    source = (ROOT/('libinterp/'+module+'.c')).read_text()
    header, declarations, callback, shapes, name, first = configuration(module, source)
    body = support['function'](source, 'void\n'+module+'modinit(')
    if control in ('face-map','glyph-map'):
        if module != 'freetype':
            raise RuntimeError('Legacy font map controls require Freetype')
        if control == 'face-map':
            old = '\tnativefieldmap(facebits, Face, freetypeface.familyname);\n\tnativefieldmap(facebits, Face, freetypeface.stylename);'
            new = '\t{ uchar legacy[] = Freetype_Face_map; memmove(facebits, legacy, sizeof(facebits)); }'
        else:
            old = '\tnativefieldmap(glyphbits, Freetype_Glyph, bitmap);'
            new = '\t{ uchar legacy[] = Freetype_Glyph_map; memmove(glyphbits, legacy, sizeof(glyphbits)); }'
        if body.count(old) != 1:
            raise RuntimeError('Font map control marker drift')
        body = body.replace(old,new,1)
    elif control:
        old, new = {
            'retirement': ('\t\tfreetype(types[i]);', '\t\tUSED(types[i]);'),
            'publication': ('\tif(types[0] == nil)', '\t'+first+' = types[0];\n\tif(types[0] == nil)'),
            'registration': ('builtinmod("$'+name+'", '+name+'modtab, '+name+'modlen) == nil',
                             'builtinmod("$'+name+'", '+name+'modtab, '+name+'modlen) != nil'),
        }[control]
        if body.count(old) != 1:
            raise RuntimeError('Graphics control marker drift: '+module+' '+control)
        body = body.replace(old, new, 1)
    fixture = (ROOT/'utils/tests/native64-graphics-init.c').read_text()
    for marker, value in (('GRAPHICS_HEADERS',header), ('GRAPHICS_DECLARATIONS',declarations),
                          ('GRAPHICS_CALLBACK',callback), ('GRAPHICS_SHAPES',shapes), ('GRAPHICS_INITIALIZER',body)):
        fixture = fixture.replace(marker, value)
    return adapter+'\n\n'+fixture


# Each strict control must reach status10, never a compiler/signal/sanitizer failure.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', choices=('prefab','freetype'), required=True)
    parser.add_argument('--limbo', type=Path, help='Host compiler for freshly generated classic-table consumer syntax checks')
    parser.add_argument('--control', choices=('retirement','publication','registration','face-map','glyph-map'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    command = shlex.split(env.get('CC', 'cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='native64-graphics-init-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        source = work/'fixture.c'
        source.write_text(unit(args.module,args.control))
        if args.limbo:
            name = 'Prefab' if args.module == 'prefab' else 'Freetype'
            header = subprocess.check_output([str(args.limbo.resolve()), '-t', name, '-I'+str(ROOT/'module'), str(ROOT/'module/runt.m')], env=env)
            (work/(args.module+'mod.h')).write_bytes(header)
            consumer = work/'consumer.c'
            consumer.write_text((ROOT/('libinterp/'+args.module+'.c')).read_text())
        for bits in ((32,) if args.control else (32,64)):
            binary = work/('word'+str(bits))
            if args.limbo:
                subprocess.run(command+(['-DDIS_WORD32=1'] if bits == 32 else [])+['-I'+str(work)]+includes+['-fsyntax-only', str(consumer)], env=env, check=True)
                print('PASS complete graphics consumer syntax:',args.module,'WORD'+str(bits),flush=True)
            subprocess.run(command+(['-DDIS_WORD32=1'] if bits == 32 else [])+includes+[str(source),'-o',str(binary)],env=env,check=True)
            result = subprocess.run(runner+[str(binary)],env=env,capture_output=True,text=True,timeout=30)
            if args.control:
                if result.returncode != 10 or 'Graphics initialization mismatch' not in result.stderr:
                    raise RuntimeError('Graphics control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS graphics init control:',args.module,args.control,arch,flush=True)
            else:
                if result.returncode:
                    raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout,end='',flush=True)


if __name__ == '__main__':
    main()
