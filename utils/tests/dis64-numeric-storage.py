#!/usr/bin/env python3
"""Independently enumerate MP-only numeric roles against each protected ref64 slot."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
RAW = runpy.run_path(str(ROOT/'utils/tests/dis64-storage-image.py'))
OPS = runpy.run_path(str(ROOT/'utils/tests/dis64-local-operands.py'))
I, W, D, IMAGE, SECTIONS = OPS['instruction'], RAW['words'], RAW['descriptor'], RAW['image'], RAW['sections']
EXIT = I('IEXIT',27)


# Record numeric memory intervals independently of the C role/map implementation.
def cases():
    rows = []
    names = list(OPS['MOVES'])+list(OPS['ARITH'])+list(OPS['BRANCH'])
    for name in names:
        kind = (OPS['MOVES']|OPS['ARITH']|OPS['BRANCH'])[name]
        width = OPS['WIDTH'][kind]
        for protected in range(4):
            for offset in (0,4,8,12,16,24):
                branch = name in OPS['BRANCH']; arithmetic = name in OPS['ARITH']
                mode = (192 if branch else 0) | (2 if branch else 0)
                sourcewidth = 4 if name in ('ISHLL','ISHRL') else width
                spans = [(offset,sourcewidth),(24,width)] if branch else [(offset,sourcewidth),(16,width)]
                valid = all(at%size == 0 and at+size <= 32 and not (at < protected*8+8 and protected*8 < at+size) for at,size in spans)
                code = W(2)+I(name,mode,source=offset,destination=0 if branch else 16,middle=24)+EXIT
                raw = IMAGE(SECTIONS(databytes=32,data=W(0),code=code,types=[D(64,8),D(32,8,bytes([0x80>>protected]))]))
                rows.append((name+'-'+str(protected)+'-'+str(offset),int(valid),raw,RAW['POLICY']))
    # All three middle-operand choices, immediate source, high-half reads and shift count width.
    for middlemode in (0,64,128,192):
        for offset in (0,4,8,12,16,24):
            valid = middlemode != 128 and (middlemode != 192 or offset >= 8)
            raw = IMAGE(SECTIONS(databytes=32,data=W(0),types=[D(80,8),D(32,8,b'\x80')],
                code=W(2)+I('IADDW',middlemode|16,source=2,middle=offset,destination=16)+EXIT))
            rows.append(('middle-'+str(middlemode)+'-'+str(offset),int(valid),raw,RAW['POLICY']))
    for name,code,valid in (
        ('exit',EXIT,1),('jump',I('IJMP',26,destination=0),1),('call',I('ICALL',27),0),
        ('fp-source',I('IMOVW',8,source=64,destination=8),0),
        ('fp-write',I('IMOVW',17,source=42,destination=64),0),
        ('ref-low-write',I('IMOVW',16,source=42,destination=0),0),
        ('ref-high-write',I('IMOVW',16,source=42,destination=4),0),
        ('shift-long-word-source',I('ISHLL',0,source=12,destination=16),1)):
        rows.append((name,valid,IMAGE(SECTIONS(databytes=32,data=W(0),types=[D(80,8),D(32,8,b'\x80')],code=W(1)+code)),RAW['POLICY']))
    base = W(2)+I('IADDW',16,source=2,destination=8)+EXIT
    rows += [('mapped-frame',0,IMAGE(SECTIONS(data=W(0),code=base,types=[D(80,8,b'\x80\x00'),D(16,8,b'\x80')])),RAW['POLICY']),
             ('missing-data',0,IMAGE(SECTIONS(code=base)),RAW['POLICY']),
             ('mapped-frame-local',1,IMAGE(SECTIONS(data=W(0),code=base,types=[D(80,8,b'\x00\xc0'),D(16,8,b'\x80')])),RAW['POLICY']),
             ('nonempty-import',0,IMAGE(SECTIONS(data=W(0),code=base,imports=[RAW['binding']()])),RAW['POLICY']),
             ('empty-import',1,IMAGE(SECTIONS(data=W(0),code=base,imports=[])),RAW['POLICY'])]
    return rows


# Preserve the independent raw verdicts and atomic-output checks from the structural harness.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control',choices=('map','frame','fp','data','interfaces'))
    args = parser.parse_args()
    header = (ROOT/'include/dis64-numeric-storage.h').read_text()
    changes = {'map':('!dis64initspan(data, (unsigned int)instruction->source, sourcewidth)','0'),
               'frame':('|| frame.map[0] != 0',''),
               'fp':('source == AFP || destination == AFP || middle == AXINF','0'),
               'data':('(image->module.databytes != 0 && (image->present&(1U<<(Dis64Data-1))) == 0)','0'),
               'interfaces':('image->imports.count != 0 || image->exports.count != 0','0')}
    if args.control:
        old,new = changes[args.control]
        if header.count(old)!=1: raise RuntimeError('numeric map marker drift')
        header = header.replace(old,new,1)
    text = (ROOT/'utils/tests/dis64-storage-image.c').read_text().replace('#include "dis64-storage-image.h"','#include "dis64-numeric-storage.h"')
    marker = '/* Execute the real joined reader'
    wrapper = '''/* Publish only a complete private numeric proof and structural image together. */
static int
numericprobe(const unsigned char *data,unsigned int bytes,const Dis64StorageLimits *limits,Dis64StorageImage *result)
{
    Dis64StorageImage view;
    Dis64CodeView code;
    if(!dis64storageimagecheck(data,bytes,limits,&view) || !dis64numericstoragecheck(&view,limits->instructions,&code)) return 0;
    *result=view;
    return 1;
}

'''
    text = text.replace(marker,wrapper+marker,1).replace('result = dis64storageimagecheck(bytes,length,&limits,&view);','result = numericprobe(bytes,length,&limits,&view);',1)
    rows = cases();stream = W(len(rows))
    for name,expected,raw,policy in rows:
        n = name.encode();stream += W(len(n))+n+W(expected,*policy,len(raw))+raw
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='numeric-map-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);(work/'dis64-numeric-storage.h').write_text(header);(work/'test.c').write_text(text);(work/'fixtures').write_bytes(stream)
        binary=work/'test'
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-I'+str(work),'-I'+str(ROOT/'include'),str(work/'test.c'),'-o',str(binary)],check=True)
        result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(work/'fixtures')],capture_output=True,text=True,timeout=60)
        if args.control:
            # The first changed row is fixed by independent fixture order, never a crash/sanitizer outcome.
            witnesses={'map':'IMOVW-0-0','frame':'mapped-frame','fp':'fp-source','data':'missing-data','interfaces':'nonempty-import'}
            if result.returncode!=10 or result.stderr.strip()!='storage mismatch: '+witnesses[args.control]+' verdict':
                raise RuntimeError('numeric control missed predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
            print('PASS numeric storage control:',args.control)
        elif result.returncode: raise RuntimeError(result.stdout+result.stderr)
        else: print(result.stdout.replace('Dis64 storage image','private numeric storage'),end='')


if __name__ == '__main__':
    main()
