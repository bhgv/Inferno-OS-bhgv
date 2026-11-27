#!/usr/bin/env python3
"""Attach real Types/MP to real native Modules and run actual cache/root/unload ownership."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep actual ownership and cache bodies; bound only native pool/jump/diagnostic services.
def unit(control):
    fn = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    source = runpy.run_path(str(ROOT/'utils/tests/native64-module-mptype.py'))['unit'](None)
    source = source.replace('#define ArenaSlots 8 ', '#define ArenaSlots 12 ',1)
    source = source.replace('int\nmain(void)', 'int\nmodule_mptype_main(void)',1)
    source = source.replace('\tmodulecontext = c;','/* Fixture leaves ordinary linkmod uninstrumented here. */',1)
    source = source.replace('static void*\nstoragemalloc(', 'static void attachmentallocation(void); // Fixture-only pre-safety allocation observation.\n\nstatic void*\nstoragemalloc(',1)
    body = fn(source,'static void*\nstoragemalloc(')
    source = source.replace(body,body.replace('\tmoduleeffects(0);','\tattachmentallocation();\n\tmoduleeffects(0);',1),1)
    load = (ROOT/'libinterp/load.c').read_text()
    bodies = '\n\n'.join(fn(load,s) for s in ('void\nfreemod(', 'void\nunload('))
    bodies += '\n'+fn((ROOT/'libinterp/link.c').read_text(),'void\ndestroylinks(')
    bodies = re.sub(r'(?<!->)\bfree\(', 'storagefree(', bodies)
    source = source.replace(fn(source,'void\nunload('),'void freedyncode(Module*);\nvoid destroylinks(Module*);\n'+bodies,1)
    controls = {
        'type': ('types[i]->ref++;','/* omitted attached Type retain */'),
        'data': ('data->ref++;','/* omitted attached MP retain */'),
        'mark': ('Setmark(data);','/* omitted attached MP barrier */'),
        'ordinal': ('target->mptype = ordinal;','target->mptype = 0;'),
        'atomic': ('types = nil;','target->ntype = source->count; types = nil;'),
        'cached': ('if(cached == target){','if(0 && cached == target){'),
    }
    if control:
        body = fn(source,'int\nattach64storage(');old,new = controls[control]
        if body.count(old)!=1: raise RuntimeError('attachment control marker drift')
        source = source.replace(body,body.replace(old,new,1),1)
    return source+'\n'+(ROOT/'utils/tests/dis64-storage-attach.c').read_text()


# Use independently serialized storage images, with MP descriptor ordinals0/1/64 and empty libraries.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control',choices=('type','data','mark','ordinal','atomic','cached'))
    args = parser.parse_args()
    oracle = runpy.run_path(str(ROOT/'utils/tests/dis64-storage-image.py'));word = oracle['words']
    rows = [row for row in oracle['cases']() if row[1] == 1]
    d = oracle['descriptor'];sections = oracle['sections'];image = oracle['image']
    rows += [('MP-ordinal0',1,image(sections(datatype=0,frame=1,types=[d(16,8,b'\x80'),d(80,8)])),oracle['POLICY']),
             ('MP-ordinal64',1,image(sections(datatype=64,types=[d(80,8)]+[d(8,8,b'\x80')]*63+[d(16,8,b'\x80')])),oracle['POLICY'])]
    stream = word(len(rows))
    for name,expected,raw,policy in rows:
        encoded = name.encode('ascii');stream += word(len(encoded))+encoded+word(expected,len(raw))+raw
    arch = os.environ.get('NATIVE64_ARCH','amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='storage-attach-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work = Path(directory);path = work/'test.c';binary = work/'test';path.write_text(unit(args.control));(work/'fixture.bin').write_bytes(stream)
        selections = [('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1'])] if args.control else [
            ('WORD32',['-DDIS_WORD32=1']),('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']),('legacy',[])]
        for label,selection in selections:
            subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+includes+selection+[str(path),'-o',str(binary)],check=True)
            result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(work/'fixture.bin')],capture_output=True,text=True,timeout=60)
            if args.control:
                stage = {'type':'attachment type retains','data':'attachment data retain','mark':'attachment mark barrier','ordinal':'attachment MP ordinal','atomic':'attachment refusal atomic','cached':'attachment preflight allocation'}[args.control]
                if result.returncode!=10 or result.stderr.strip()!='storage owner mismatch: '+stage:
                    raise RuntimeError('attachment control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS Dis64 attachment control:',args.control,arch)
            elif result.returncode: raise RuntimeError(result.stdout+result.stderr)
            else: print(label+': '+result.stdout,end='')


if __name__ == '__main__':
    main()
