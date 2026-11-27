#!/usr/bin/env python3
"""Clone rooted Dis64 data through actual newmp/arraycpy, protected rollback and collector polls."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Observe pre-safety boundaries without modifying any production predicate in a positive run.
def unit(control):
    support = runpy.run_path(str(ROOT/'utils/tests/dis64-storage-owner.py'))
    fn = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    source = support['unit'](None,clones=True)
    source = source.replace('static int checks,', 'extern char exNegsize[];\nvoid krethrow(void);\nstatic int checks,',1)
    source = source.replace('int\nmain(int argc, char **argv)', 'int\nstorage_owner_main(int argc, char **argv)',1)
    copy = fn(source,'static int\ndis64storagecopy(')
    observed = copy.replace('\tif(copy->datatype != nil){','\tcloneobserve(owner,0);\n\tif(copy->datatype != nil){',1)
    observed = observed.replace('\t\tnewmp(owner->data, source->data, copy->datatype);','\t\tcloneobserve(owner,1);\n\t\tnewmp(owner->data, source->data, copy->datatype);',1)
    source = source.replace(copy,observed,1)
    source = source.replace('static int\ndis64storagecopy(', 'static void cloneobserve(Dis64StorageOwner*,int); // Fixture-only pre-safety observation, absent from production.\n\nstatic int\ndis64storagecopy(',1)
    pool = fn(source,'void*\npoolalloc(')
    source = source.replace(pool,'static void cloneallocationpoll(void); // Fixture-only collector scheduling at actual pool boundaries.\n\n'+pool.replace('\tfor(i=1; i<ArenaSlots; i++){','\tcloneallocationpoll();\n\tfor(i=1; i<ArenaSlots; i++){',1),1)
    mutations = {
        'retain': ('static int\ndis64storagecopy(', 'owner->types[i]->ref++;', '/* omitted table retain */'),
        'pin': ('static int\ndis64storagecopy(', 'ptradd(heap);', '/* omitted clone pin */'),
        'copy': ('static int\ndis64storagecopy(', 'newmp(owner->data, source->data, copy->datatype);', '/* omitted recursive data copy */'),
        'release': ('void\nfree64storage(', 'ptrdel(D2H(data));', '/* omitted owned root retirement */'),
        'partial': ('Dis64StorageOwner*\nclone64storage(', 'free64storage(copy.owner);', '/* omitted partial clone cleanup */'),
        'reserve': ('Dis64StorageOwner*\nclone64storage(', '0x7ffffffe - (type == copy.datatype)', '0x7ffffffe'),
    }
    if control:
        signature,old,new = mutations[control]
        body = fn(source,signature)
        if body.count(old)!=1: raise RuntimeError('clone control marker drift')
        source = source.replace(body,body.replace(old,new,1),1)
    source += '\n'+fn((ROOT/'emu/port/dis.c').read_text(),'void\nkrethrow(')
    return source+'\n'+(ROOT/'utils/tests/dis64-storage-clone.c').read_text()


# Accepted raw storage images are not executable permissions; keep fixed independent bytes as inputs.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control',choices=('retain','pin','copy','release','partial','reserve'))
    args = parser.parse_args()
    oracle = runpy.run_path(str(ROOT/'utils/tests/dis64-storage-image.py'))
    rows = [row for row in oracle['cases']() if row[1] == 1]
    d = oracle['descriptor'];sections = oracle['sections'];image = oracle['image'];word = oracle['words']
    rows += [('MP-ordinal0',1,image(sections(datatype=0,frame=1,types=[d(16,8,b'\x80'),d(80,8)])),oracle['POLICY']),
             ('MP-ordinal64',1,image(sections(datatype=64,types=[d(80,8)]+[d(8,8,b'\x80')]*63+[d(16,8,b'\x80')])),oracle['POLICY'])]
    word = oracle['words'];stream = word(len(rows))
    for name,expected,raw,policy in rows:
        encoded = name.encode('ascii');stream += word(len(encoded))+encoded+word(expected,len(raw))+raw
    arch = os.environ.get('NATIVE64_ARCH','amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='storage-clone-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work = Path(directory);source = work/'test.c';binary = work/'test'
        source.write_text(unit(args.control));(work/'fixture.bin').write_bytes(stream)
        selections = [('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1'])] if args.control else [
            ('WORD32',['-DDIS_WORD32=1']),('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']),('legacy',[])]
        for label,selection in selections:
            subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+includes+selection+[str(source),'-o',str(binary)],check=True)
            result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(work/'fixture.bin')],capture_output=True,text=True,timeout=60)
            if args.control:
                stage = {'retain':'clone type retains','pin':'clone pin','copy':'clone scalar bits','release':'clone cleanup baseline','partial':'clone refusal baseline','reserve':'MP retain preflight'}[args.control]
                if result.returncode!=10 or result.stderr.strip()!='storage owner mismatch: '+stage:
                    raise RuntimeError('clone control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS Dis64 storage clone control:',args.control,arch)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(label+': '+result.stdout,end='')


if __name__ == '__main__':
    main()
