#!/usr/bin/env python3
"""Check a private local-call metadata proposal without admitting any bytecode."""
import argparse
import itertools
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Encode fixture words independently of production readers/writers.
def words(*values):
    return struct.pack('>'+'I'*len(values), *values)


# Compute proposed scalar spans using Python integers and independent alignment.
def geometry(parameters):
    at = 64
    for kind in parameters:
        width = {1: 4, 2: 8, 3: 8, 4: 4}[kind]
        at = ((at+width-1)//width)*width+width
    return at


# Construct already-agreed pool/type/export framing around independent function records.
def fixture(records, sizes, *, code=256, entry=0, frame=0, cap=64, limit=1048576, names=None, maps=None, alignments=None, reject=False):
    names = names or ['function'+str(i) for i in range(len(records))]
    pool = words(len(names))+b''.join(words(len(name), *(ord(c) for c in name)) for name in names)
    maps = maps or [bytes((size+63)//64) for size in sizes]
    alignments = alignments or [8]*len(sizes)
    types = words(len(sizes))+b''.join(words(size, alignment)+mapping for size, alignment, mapping in zip(sizes, alignments, maps))
    exports = words(len(records))
    for symbol, pc, kind, result, parameters in records:
        signature = words(1, result, len(parameters), *parameters)
        exports += words(symbol, pc, kind, len(signature))+signature
    expected = []
    if not reject:
        ordered = sorted(records, key=lambda record: record[1])
        for i, (symbol, pc, kind, result, parameters) in enumerate(ordered):
            end = ordered[i+1][1] if i+1 < len(ordered) else code
            expected += list(words(symbol, pc, end, kind, geometry(parameters), sizes[kind]-8, result, len(parameters), *parameters))
    return [exports, pool, types, limit, code, entry, frame, cap, len(records) if not reject else 0, bytes(expected)]


# Keep success oracles distinct from malformed-byte and proposed-policy refusals.
def cases():
    catalog = []
    rng = random.Random(73)
    for count in range(1, 17):
        records = [(0, 0, 0, 0, [])]
        sizes = [80]
        for i in range(1, count):
            parameters = [1+(i+j)%4 for j in range(i%7)]
            records.append((i, 3*i, i, 1+i%4, parameters))
            sizes.append(((geometry(parameters)+7)//8)*8+8+8*(i%3))
        for _ in range(6):
            rng.shuffle(records)
            catalog.append(fixture(records, sizes))
    for parameters in itertools.product(range(1, 5), repeat=3):
        end = geometry(parameters)
        records = [(1, 4, 1, 2, parameters), (0, 0, 0, 0, [])]
        catalog.append(fixture(records, [80, ((end+7)//8)*8+8]))
        catalog.append(fixture(records, [80, ((end+7)//8)*8], reject=True))
    for count in range(65):
        parameters = [1+(i%4) for i in range(count)]
        end = geometry(parameters)
        for padding in (0, 8, 24):
            catalog.append(fixture([(0, 0, 0, 0, []), (1, 10, 1, 3, parameters)], [80, ((end+7)//8)*8+8+padding]))
    catalog.append(fixture([(0,0,0,0,[])],[72],code=1,cap=1))
    catalog.append(fixture([(2,9,1,3,[3]),(0,0,0,0,[]),(1,4,1,1,[1])],[80,80]))
    base = [(0, 0, 0, 0, []), (1, 4, 1, 1, [1])]
    valid = fixture(base, [80, 80])
    for section in range(3):
        for length in range(len(valid[section])):
            changed = valid.copy()
            changed[section] = valid[section][:length]
            changed[8] = 0
            changed[9] = b''
            catalog.append(changed)
        changed = valid.copy(); changed[section] += b'\0'; changed[8] = 0; changed[9] = b''
        catalog.append(changed)
    # Corrupt the existing signature/name/type records independently of draft geometry.
    for offset, value in ((4,99),(12,99),(16,0xffffffff),(20,2),(24,99),(28,65)):
        changed=valid.copy(); blob=bytearray(changed[0]); blob[offset:offset+4]=words(value)
        changed[0]=bytes(blob);changed[8]=0;changed[9]=b'';catalog.append(changed)
    catalog.append(fixture(base,[80,80],names=['entry','ж']))
    for code in (0, 4, 65537, 0xffffffff):
        catalog.append(fixture(base, [80, 80], code=code, reject=True))
    catalog.append(fixture([(0,0,0,0,[]),(1,65535,1,1,[])], [80,80], code=65536))
    for entry, frame, cap in ((1,0,64),(0,1,64),(0,99,64),(0,0,1),(0,0,0)):
        catalog.append(fixture(base,[80,80],entry=entry,frame=frame,cap=cap,reject=True))
    for size in (64, 68, 72):
        catalog.append(fixture(base,[80,size],alignments=[8,4 if size==68 else 8],reject=True))
    for result, parameters in ((1,[]),(0,[1])):
        catalog.append(fixture([(0,0,0,result,parameters),(1,4,1,1,[])],[80,80],reject=True))
    catalog.append(fixture([(0,0,0,0,[]),(1,0,1,1,[])],[80,80],reject=True))
    catalog.append(fixture(base,[80,80],names=['same','same'],reject=True))
    catalog.append(fixture(base,[80,80],maps=[bytes(2),bytes([0,0x80])],reject=True))
    catalog.append(fixture(base,[80,80],limit=79,reject=True))
    catalog.append(fixture([],[],reject=True))
    for count in (64,65):
        records=[(i,2*i,i,0 if i==0 else 1,[]) for i in range(count)]
        catalog.append(fixture(list(reversed(records)),[80]*count,reject=count==65))
    catalog.append(fixture([(0,0,0,0,[]),(1,4,1,1,[1]*65)],[80,336],reject=True))
    catalog.append(fixture([(0,0,0,0,[]),(1,4,1,1,[])],[80,65536],limit=65536))
    catalog.append(fixture([(0,0,0,0,[]),(1,4,1,1,[])],[80,65536],limit=65535,reject=True))
    return catalog


# Run the same portable C metadata checker natively or through the target runner.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('entries','tail'))
    args = parser.parse_args()
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC','cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER',''))
    inputs = cases()
    with tempfile.TemporaryDirectory(prefix='dis64-local-catalog-',dir=env.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);env['TMPDIR']=str(work)
        header=(ROOT/'include/dis64-local-catalog.h').read_text()
        if args.control=='entries':
            header=header.replace('if(i != 0 && functions[i-1].entry == functions[i].entry)','if(0)')
        elif args.control=='tail':
            header=header.replace('tail = frame.bytes-8;','tail = frame.bytes;').replace('function.resultcell = tail;','function.resultcell = frame.bytes-8;')
        (work/'dis64-local-catalog.h').write_text(header)
        data=words(len(inputs))
        for exports,pool,types,limit,code,entry,frame,cap,count,expected in inputs:
            data+=words(len(exports),len(pool),len(types),limit,code,entry,frame,cap,count)+exports+pool+types+expected
        fixturepath=work/'cases.bin';fixturepath.write_bytes(data)
        binary=work/'catalog'
        subprocess.run(compiler+flags+['-I'+str(work),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-local-catalog.c'),'-o',str(binary)],env=env,check=True)
        result=subprocess.run(runner+[str(binary),str(fixturepath)],env=env,capture_output=True,text=True,timeout=90)
        if args.control:
            if result.returncode!=10 or 'local catalog predicate failed' not in result.stderr:
                raise RuntimeError('catalog control missed predicate: '+result.stdout+result.stderr)
            print('PASS experimental catalog removal control:',args.control)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout,end='')


if __name__=='__main__':
    main()
