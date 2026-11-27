#!/usr/bin/env python3
"""Independent raw-byte joins for Dis64 metadata/types/DATA/interfaces; no execution."""
import argparse
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
POLICY = (16, 128, 4096, 32, 64, 64, 16, 64)


# Write fixed-width big-endian fields independently of every production writer.
def words(*values):
    return struct.pack('>'+'I'*len(values), *values)


# Build the accepted envelope directly, without the C checker/writer as an oracle.
def image(sections):
    at = 32+12*len(sections)
    directory = b''
    payload = b''
    for kind, body in sections:
        directory += struct.pack('>HHII',kind,1 if kind < 9 else 0,at,len(body))
        payload += body
        at += len(body)
    return b'Dis64\r\n\x1a'+struct.pack('>HHIIIII',1,0,32+12*len(sections),1,0,0,len(sections))+directory+payload


# Structural type records include exact full map8, also for unused managed objects.
def descriptor(size, align, bitmap=None):
    return words(size,align)+(bytes((size+63)//64) if bitmap is None else bitmap)


# Versioned scalar signatures encode kinds, not host types or hashes.
def signature(result=0, parameters=()):
    return words(1,result,len(parameters),*parameters)


# Bindings use actual pool ordinals, including separately numbered equal contents.
def binding(module=3, symbol=1, export=False, entry=0, frame=0, sig=None):
    sig = signature() if sig is None else sig
    return (words(symbol,entry,frame,len(sig)) if export else words(module,symbol,len(sig)))+sig


# Compose raw structural facts with independently selected mutation sites.
def sections(profile=1, databytes=16, data=None, imports=None, exports=None, types=None,
             entry=0, frame=0, name=0, datatype=1, code=None):
    module = words(name,profile,databytes)+(words(datatype) if databytes else b'')
    if profile == 1:
        module += words(entry,frame)
    types = [descriptor(80,8),descriptor(16,8,b'\x80')] if types is None else types
    pool = ['unit','symbol','symbol','module','module','other']
    strings = words(len(pool))+b''.join(words(len(s),*(ord(c) for c in s)) for s in pool)
    result = [(1,module),(2,words(1)+bytes((15,27)) if code is None else code),
              (3,words(len(types))+b''.join(types)),(7,strings)]
    if data is not None:
        result.append((4,data))
    if imports is not None:
        result.append((5,words(len(imports))+b''.join(imports)))
    if exports is not None:
        result.append((6,words(len(exports))+b''.join(exports)))
    return result


# Expected outcomes follow the agreed structural rules; execution roles stay deliberately unproved.
def cases():
    rows = []
    add = lambda name, expected, raw, policy=POLICY: rows.append((name,expected,raw,policy))
    initialized = words(1,8,1,0x80000000)
    good = image(sections(data=initialized,imports=[binding(),binding(module=4,symbol=2)],
                          exports=[binding(export=True)]))
    add('managed-storage',1,good)
    add('unused-managed-and65-types',1,image(sections(types=[descriptor(80,8),descriptor(16,8,b'\x80')]+[descriptor(8,8,b'\x80')]*63)))
    add('absent-data-is-absent',1,image(sections()))
    add('empty-interfaces',1,image(sections(imports=[],exports=[])))
    add('library-empty-code',1,image(sections(profile=0,databytes=0,types=[],code=words(0))))
    add('library-export',1,image(sections(profile=0,exports=[binding(export=True)])))
    add('different-modules',1,image(sections(imports=[binding(),binding(module=5,sig=signature(1))])))
    add('distinct-exports',1,image(sections(exports=[binding(export=True),binding(export=True,symbol=5)])))
    add('empty-data-zero-object',1,image(sections(databytes=0,data=words(0))))
    add('opaque-optional',1,image(sections()+[(50000,b'opaque')]))
    for kind in (2,3):
        for high,low in ((0,0),(0x80000000,0),(0x7ff80000,1),(0xffffffff,0xffffffff)):
            add('wide-bits-'+str(kind)+'-'+str(high),1,image(sections(data=words(1,8,kind,high,low))))
    add('bool-one',1,image(sections(data=words(1,8,4,1))))
    add('bool-zero',1,image(sections(data=words(1,12,4,0))))
    add('unsorted-disjoint-data',1,image(sections(data=words(2,12,1,17,8,1,23))))
    add('managed-entry-frame-structural-only',1,image(sections(types=[descriptor(80,8,b'\x80\x00'),descriptor(16,8,b'\x80')])) )
    # These exact-stage rows are omission-control witnesses.
    add('bad-unused-type',0,image(sections(types=[descriptor(80,8),descriptor(16,8,b'\x80'),descriptor(8,3)])))
    add('data-overlap',0,image(sections(data=words(2,8,1,7,8,1,9))))
    add('import-conflict',0,image(sections(imports=[binding(),binding(module=4,symbol=2,sig=signature(1))])))
    add('export-duplicate',0,image(sections(exports=[binding(export=True),binding(export=True,symbol=2)])))
    add('zero-pc-library-export-refusal',0,image(sections(profile=0,code=words(0),exports=[binding(export=True)])))
    add('code-trailer',0,image(sections(code=words(1)+bytes((15,27,0)))))
    add('data-size-mismatch',0,image(sections(databytes=24)))
    add('missing-data-type',0,image(sections(datatype=2)))
    add('missing-name',0,image(sections(name=8)))
    add('entry-outside',0,image(sections(entry=1)))
    add('entry-frame-missing',0,image(sections(frame=2)))
    add('entry-frame-short',0,image(sections(frame=1)))
    add('entry-frame-align',0,image(sections(types=[descriptor(80,4),descriptor(16,8,b'\x80') ])))
    add('data-mapped-low',0,image(sections(data=words(1,0,1,1))))
    add('data-mapped-high',0,image(sections(data=words(1,4,1,1))))
    add('data-bool',0,image(sections(data=words(1,8,4,2))))
    add('data-outside',0,image(sections(data=words(1,16,1,1))))
    add('data-kind',0,image(sections(data=words(1,8,0,1))))
    add('data-misaligned',0,image(sections(data=words(1,9,1,1))))
    add('data-trailer',0,image(sections(data=initialized+b'\x00')))
    add('data-nonempty-zero-object',0,image(sections(databytes=0,data=initialized)))
    add('export-frame-missing',0,image(sections(exports=[binding(export=True,frame=2)])))
    add('export-pc-outside',0,image(sections(exports=[binding(export=True,entry=1)])))
    add('import-name-missing',0,image(sections(imports=[binding(module=6)])))
    add('export-name-missing',0,image(sections(exports=[binding(export=True,symbol=6)])))
    add('signature-version',0,image(sections(imports=[binding(sig=words(2,0,0))])))
    add('control-unsupported',0,image(sections()+[(8,words(0))]))
    add('opcode-unknown',0,image(sections(code=words(1)+bytes((255,27)))))
    add('instruction-count',0,image(sections(code=words(2)+bytes((15,27)))))
    add('missing-strings',0,image([s for s in sections() if s[0]!=7]))
    add('duplicate-data',0,image(sections(data=words(0))+[(4,words(0))]))
    for n in range(len(good)):
        add('truncated-'+str(n),0,good[:n])
    # Explicit policy is not a wire-format limit; each field is exercised independently.
    for index, limit in enumerate((6,1,79,5,0,1,0,0)):
        p = list(POLICY);p[index] = limit
        source = good
        if index == 6:
            source = image(sections(imports=[binding(sig=signature(parameters=(1,)))]))
        add('policy-'+str(index),0,source,tuple(p))
    # Section order is immaterial when all offsets/contents remain correctly joined.
    for shift in range(7):
        parts = sections(data=initialized,imports=[],exports=[binding(export=True)])
        add('directory-rotation-'+str(shift),1,image(parts[shift:]+parts[:shift]))
    return rows


# Compile the actual reader; omission controls must hit only their exact semantic witness.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control',choices=('types','data','imports','exports','code','module'))
    args = parser.parse_args()
    header = (ROOT/'include/dis64-storage-image.h').read_text()
    controls = {
        'types': ('if(!dis64typevalid(&type))','if(i+1 != view.count && !dis64typevalid(&type))','bad-unused-type'),
        'data': ('if(!dis64initializerscheck(', 'if(0 && !dis64initializerscheck(', 'data-overlap'),
        'imports': ('if((image.present&(1U<<(i-1))) == 0)', 'if(i == Dis64Imports || (image.present&(1U<<(i-1))) == 0)', 'import-conflict'),
        'exports': ('if((image.present&(1U<<(i-1))) == 0)', 'if(i == Dis64Exports || (image.present&(1U<<(i-1))) == 0)', 'export-duplicate'),
        'code': ('if(at != bytes)', 'if(0 && at != bytes)', 'code-trailer'),
        'module': ('image.data.bytes != image.module.databytes', '0', 'data-size-mismatch'),
    }
    rows = cases()
    stream = words(len(rows))
    for name, expected, raw, policy in rows:
        encoded = name.encode('ascii')
        stream += words(len(encoded))+encoded+words(expected,*policy,len(raw))+raw
    arch = os.environ.get('NATIVE64_ARCH','amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='storage-image-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work = Path(directory)
        if args.control:
            old,new,witness = controls[args.control]
            if args.control == 'types':
                body = (ROOT/'include/dis64-types.h').read_text()
                if body.count(old)!=1: raise RuntimeError('type marker drift')
                (work/'dis64-types.h').write_text(body.replace(old,new,1))
                # Relative includes must resolve the controlled descriptor codec first.
                for name in ('dis64-bindings.h','dis64-data.h'):
                    (work/name).write_text((ROOT/'include'/name).read_text())
            else:
                if header.count(old)!=1: raise RuntimeError('storage marker drift')
                header = header.replace(old,new,1)
        (work/'dis64-storage-image.h').write_text(header)
        (work/'fixture.bin').write_bytes(stream)
        binary = work/'test'
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-I'+str(work),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-storage-image.c'),'-o',str(binary)],check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(work/'fixture.bin')],capture_output=True,text=True,timeout=60)
        if args.control:
            expected = 'storage mismatch: '+witness+' verdict'
            if result.returncode!=10 or result.stderr.strip()!=expected:
                raise RuntimeError('storage control missed semantic witness: '+str(result.returncode)+' '+result.stdout+result.stderr)
            print('PASS Dis64 storage control:',args.control,arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout,end='')


if __name__ == '__main__':
    main()
