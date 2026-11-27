#!/usr/bin/env python3
"""Run real module instance construction with explicit MP ordinals and collector/error ownership."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep actual heap/collector/link statements and expose pre-safety observations only in the fixture.
def unit(control):
    support = runpy.run_path(str(ROOT/'utils/tests/dis64-storage-clone.py'))
    fn = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    source = support['unit'](None).replace('int\nmain(int argc, char **argv)','int\nstorage_clone_main2(int argc, char **argv)',1)
    source = source.replace('static void*\nstoragemalloc(', 'static void moduleeffects(int); // Fixture-only pre-safety effect observation.\n\nstatic void*\nstoragemalloc(',1)
    body = fn(source,'static void*\nstoragemalloc(')
    source = source.replace(body,body.replace('\tif(++operation == failoperation)','\tmoduleeffects(0);\n\tif(++operation == failoperation)',1),1)
    body = fn(source,'void*\npoolalloc(')
    source = source.replace(body,body.replace('\tif(++operation == failoperation)','\tmoduleeffects(1);\n\tif(++operation == failoperation)',1),1)
    heap = (ROOT/'libinterp/heap.c').read_text()
    native = 'void freemodlink(Heap*,int);\nvoid freedyndata(Modlink*);\n'+next(line for line in heap.splitlines() if line.startswith('Type\tTmodlink'))+'\n'+fn(heap,'void\nfreemodlink(')
    link = (ROOT/'libinterp/link.c').read_text()
    native += '\n'+fn(link,'static int\nlinkm(')+'\n'+fn(link,'Modlink*\nmklinkmod(')+'\n'+link[link.index('typedef struct LinkModule'):link.index('/* Release partial or complete export')]
    native = native.replace('\tm = c->m;','\tm = c->m;\n\tmodulecontext = c;',1)
    native = native.replace('typedef struct LinkModule LinkModule;','typedef struct LinkModule LinkModule;\nstatic LinkModule *modulecontext; // Fixture-only stable constructor observation.',1)
    mutations = {
        'ordinal': ('t = m->type[m->mptype];','t = m->type[0];'),
        'descriptor': ('D2H(m->origmp)->ref == 0 || D2H(m->origmp)->t != t','D2H(m->origmp)->ref == 0'),
        'source': ('ptradd(D2H(c->source));','/* omitted source pin */'),
        'instance': ('ptradd(D2H(ml));','/* omitted instance pin */'),
    }
    if control:
        old,new = mutations[control]
        if native.count(old)!=1: raise RuntimeError('MP ordinal control marker drift')
        native = native.replace(old,new,1)
    tests = (ROOT/'utils/tests/native64-module-mptype.c').read_text().replace('static LinkModule *modulecontext; // Stable production constructor context only while its protected body runs.\n','')
    return source+'\n'+native+'\n'+tests


# Preserve native compiler failures separately from exact semantic guard/ownership controls.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control',choices=('ordinal','descriptor','source','instance'))
    args = parser.parse_args()
    arch = os.environ.get('NATIVE64_ARCH','amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='module-mptype-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work = Path(directory);source = work/'test.c';binary = work/'test';source.write_text(unit(args.control))
        for label,selection in ([('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1'])] if args.control else [('WORD32',['-DDIS_WORD32=1']),('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1'])]):
            subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+includes+selection+[str(source),'-o',str(binary)],check=True)
            result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=60)
            if args.control:
                stage = {'ordinal':'module ordinal success','descriptor':'module descriptor preflight','source':'module source pin','instance':'module instance pin'}[args.control]
                if result.returncode!=10 or result.stderr.strip()!='storage owner mismatch: '+stage:
                    raise RuntimeError('MP ordinal control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS native Module MP control:',args.control,arch)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else: print(label+': '+result.stdout,end='')


if __name__ == '__main__':
    main()
