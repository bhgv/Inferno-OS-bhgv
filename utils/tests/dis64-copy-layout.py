#!/usr/bin/env python3
"""Run production bounded copy lowering against memmove, including overlapping storage and an active ordering control."""
from pathlib import Path
import runpy
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract the actual emitter and shared role checker; compare emitted operations against libc's independent reference.
def main():
    extract = runpy.run_path(str(ROOT/'utils/tests/dis64-tuple-layout.py'))['function']
    source = (ROOT/'limbo/dis64-lower.c').read_text()
    production = '\n\n'.join(extract(source, name) for name in
                            ('dis64address', 'dis64copywidth', 'dis64copyspan', 'dis64copyemit'))
    fixture = r'''
#include "limbo.h"
#include "dis64-lower.h"
#include <limits.h>
#include <stdio.h>
PRODUCTION


/* Execute actual emitted Word moves and compare every public byte with independent memmove semantics. */
static int
check(int source, int destination, int from, int to, unsigned int bytes)
{
    unsigned int mp[32], fp[64], expectedmp[32], expectedfp[64], i, count, sm, dm;
    unsigned char *s, *d, *es, *ed;
    Dis64Instruction code[32];
    Inst *origin[32], input = {0};
    Dis64Lower lower = {0};

    for(i=0; i<32; i++)mp[i] = expectedmp[i] = 0xa5000000U+i;
    for(i=0; i<64; i++)fp[i] = expectedfp[i] = 0x5a000000U+i;
    memset(code, 0, sizeof(code)); memset(origin, 0, sizeof(origin));
    input.op = IMOVM; input.mm = Aimm; input.m.offset = bytes;
    input.sm = source; input.s.reg = from; input.dm = destination; input.d.reg = to;
    count = 2*(bytes/4); lower.code = code; lower.origin = origin; lower.count = count;
    if(!dis64copyemit(&input,128,256,176,&lower,0))return 1;
    from = source==Afp ? from+32 : from;
    to = destination==Afp ? to+32 : to;
    es = (unsigned char*)(source==Afp ? expectedfp : expectedmp)+from;
    ed = (unsigned char*)(destination==Afp ? expectedfp : expectedmp)+to;
    memmove(ed,es,bytes);
    for(i=0; i<count; i++){
        if(code[i].op!=IMOVW || !dis64instructioncheck(&code[i],count,128,256) || origin[i]!=&input)return 2;
        sm = USRC(code[i].mode); dm = UDST(code[i].mode);
        if((sm!=AFP && sm!=AMP) || (dm!=AFP && dm!=AMP))return 3;
        s = (unsigned char*)(sm==AFP ? fp : mp)+code[i].source;
        d = (unsigned char*)(dm==AFP ? fp : mp)+code[i].destination;
        memmove(d,s,4);
    }
    if(memcmp(mp,expectedmp,sizeof(mp)) || memcmp(fp,expectedfp,192))return 10;
    return 0;
}


/* Bound both logical source spans and the private snapshot; reject malformed sizes and unsafe modes. */
int
main(void)
{
    Inst input = {0};
    Addr address = {0};
    Dis64Instruction code[32];
    Inst *origin[32];
    Dis64Lower lower = {0};
    unsigned int sizes[] = {4,8,12,24,64}, i;
    int result;

    for(i=0; i<5; i++){
        result=check(Amp,Amp,0,4,sizes[i]); if(result)return result;
        result=check(Amp,Amp,4,0,sizes[i]); if(result)return result;
        result=check(Amp,Amp,0,0,sizes[i]); if(result)return result;
        result=check(Amp,Amp,0,64,sizes[i]); if(result)return result;
        result=check(Amp,Afp,0,32,sizes[i]); if(result)return result;
        result=check(Afp,Amp,32,0,sizes[i]); if(result)return result;
        result=check(Afp,Afp,32,36,sizes[i]); if(result)return result;
        result=check(Afp,Afp,36,32,sizes[i]); if(result)return result;
    }
    input.op = IMOVM; input.mm = Aimm; input.m.offset = 64;
    if(dis64copywidth(&input)!=64)return 11;
    input.m.offset = 3; if(dis64copywidth(&input))return 12;
    input.m.offset = 68; if(dis64copywidth(&input))return 13;
    input.m.offset = -4; if(dis64copywidth(&input))return 14;
    input.m.offset = 8; input.mm = Afp; if(dis64copywidth(&input))return 15;
    input.mm = Aimm; input.branch = &input; if(dis64copywidth(&input))return 16;
    input.branch = nil; input.op = IMOVMP; if(dis64copywidth(&input))return 17;
    address.reg = 32;
    if(!dis64copyspan(Afp,&address,64,96,128))return 18;
    if(dis64copyspan(Afp,&address,64,95,128))return 19;
    address.reg = 20;
    if(!dis64copyspan(Afp,&address,12,96,128) || dis64copyspan(Afp,&address,16,96,128))return 20;
    address.reg = 16; if(dis64copyspan(Afp,&address,4,96,128))return 21;
    address.reg = 0; if(dis64copyspan(Afpind,&address,4,96,128))return 22;
    input.op = IMOVM; input.mm = Aimm; input.m.offset = 8;
    input.sm = Amp; input.dm = Afp; input.s.reg = 0; input.d.reg = 32;
    lower.code = code; lower.origin = origin; lower.count = 4;
    if(dis64copyemit(&input,128,255,176,&lower,0))return 23;
    if(dis64copyemit(&input,128,256,~0U,&lower,0))return 24;
    input.d.reg = 160; if(dis64copyemit(&input,128,256,176,&lower,0))return 25;
    input.d.reg = 32; input.s.reg = LONG_MAX;
    if(dis64copyemit(&input,128,256,176,&lower,0))return 26;
    input.s.reg = 0; lower.count = 3;
    if(dis64copyemit(&input,128,256,176,&lower,0))return 27;
    puts("PASS production copy emitter: 40 memmove comparisons, overlap directions/self alias, FP/MP, size/span/snapshot/header/count refusals");
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory(prefix='dis64-copies-') as directory:
        work = Path(directory)
        for control in (False, True):
            actual = fixture
            if control:
                actual = actual.replace('at+words+i', 'at+2*i+1').replace('at+i', 'at+2*i')
                if actual==fixture:
                    raise RuntimeError('Inactive snapshot-order control')
            target, binary = work/'test.c', work/'test'
            target.write_text(actual)
            command = ['cc', '-O2', '-g', '-fsanitize=undefined', '-fno-sanitize-recover=all']
            command += ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'include', 'limbo')]
            subprocess.run(command+[str(target), '-o', str(binary)],check=True)
            result = subprocess.run([str(binary)])
            if result.returncode != (10 if control else 0):
                raise RuntimeError('Invalid copy/snapshot control outcome: '+str(result.returncode))
    print('PASS active copy-order control: interleaved writes corrupt a forward-overlapping source')


if __name__=='__main__':
    main()
