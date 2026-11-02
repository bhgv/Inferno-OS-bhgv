#!/usr/bin/env python3
"""Exercise actual compiler tuple geometry, result producers and caller binding under UBSan."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Borrow complete production functions, keeping their real Type/Decl/Inst records and shared scalar validator.
def function(source, name):
    match = re.search(r'(?m)^(?:static )?(?:unsigned int|int|Type\*)\n'+name+r'\(', source)
    if match is None:
        raise RuntimeError('Missing production function: '+name)
    opening = source.index('{', match.start())
    depth, end = 1, opening+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}')
        end += 1
    return source[match.start():end]


# Require real runtime-check failures after removing field identity or caller-frame alias protection.
def main():
    source = (ROOT/'limbo/dis64-inline.c').read_text()
    lower = (ROOT/'limbo/dis64-lower.c').read_text()
    names = ('inlinewidth', 'dis64inlineshape', 'inlinefield', 'inlinesameresult',
             'inlineargument', 'inlineproducer', 'inlineresultproducer', 'inlinebinding')
    production = '\n\n'.join(function(source, name) for name in names)
    production += '\n\n'+function(lower, 'dis64address')+'\n\n'+function(lower, 'dis64scalarinst')
    defines = '\n'.join(re.findall(r'(?m)^#define ScalarInlineResult.*$', source))
    fixture = r'''
#include "limbo.h"
#include "dis64-lower.h"
DEFINES
PRODUCTION


/* Check independent canonical layouts, padding, field identity, storage limits and actual result bindings. */
int
main(void)
{
    Type word = {0}, big = {0}, real = {0}, managed = {0}, tuple = {0}, clone, boundary = {0};
    Decl fields[3] = {{0}}, other[3] = {{0}}, many[9] = {{0}};
    Inst frame = {0}, binding = {0}, producer = {0};
    int i;

    word.kind = Tint; word.size = word.align = 4;
    big.kind = Tbig; big.size = big.align = 8;
    real.kind = Treal; real.size = real.align = 8;
    managed.kind = Tstring;
    tuple.kind = Ttuple; tuple.align = 8; tuple.size = 24; tuple.ids = fields;
    fields[0].ty = &word; fields[0].offset = 0; fields[0].next = &fields[1];
    fields[1].ty = &big; fields[1].offset = 8; fields[1].next = &fields[2];
    fields[2].ty = &real; fields[2].offset = 16;
    if(dis64inlineshape(&word)!=4 || dis64inlineshape(&tuple)!=24)return 1;
    if(inlinefield(&tuple,0)!=&word || inlinefield(&tuple,8)!=&big || inlinefield(&tuple,16)!=&real)return 2;
    if(inlinefield(&tuple,4)!=nil || inlinefield(&tuple,20)!=nil || inlinefield(&tuple,24)!=nil)return 3;
    fields[1].offset = 4;
    if(dis64inlineshape(&tuple)!=0)return 4;
    fields[1].offset = 16;
    if(dis64inlineshape(&tuple)!=0)return 5;
    fields[1].offset = 8; fields[1].ty = &managed;
    if(dis64inlineshape(&tuple)!=0)return 6;
    fields[1].ty = &tuple;
    if(dis64inlineshape(&tuple)!=0)return 7;
    fields[1].ty = &big; tuple.size = 20;
    if(dis64inlineshape(&tuple)!=0)return 8;
    tuple.size = 24; tuple.align = 4;
    if(dis64inlineshape(&tuple)!=0)return 9;
    tuple.align = 8;
    for(i=0; i<9; i++){
        many[i].ty = &big; many[i].offset = i*8;
        many[i].next = i<7 ? &many[i+1] : nil;
    }
    boundary.kind = Ttuple; boundary.align = 8; boundary.size = 64; boundary.ids = many;
    if(dis64inlineshape(&boundary)!=64)return 10;
    for(i=0; i<9; i++){
        many[i].ty = &word; many[i].offset = i*4;
        many[i].next = i<8 ? &many[i+1] : nil;
    }
    boundary.align = 4; boundary.size = 36;
    if(dis64inlineshape(&boundary)!=0)return 11;
    fields[0].next = &fields[0];
    if(dis64inlineshape(&tuple)!=0)return 12;
    fields[0].next = &fields[1];
    for(i=0; i<3; i++){
        other[i] = fields[i]; other[i].next = i<2 ? &other[i+1] : nil;
    }
    clone = tuple; clone.ids = other;
    if(!inlinesameresult(&tuple,&clone))return 13;
    other[1].ty = &real; other[2].ty = &big;
    if(inlinesameresult(&tuple,&clone))return 14;
    producer.op = IMOVL; producer.mm = Anone; producer.dm = Afpind;
    producer.d.reg = ScalarInlineResult; producer.d.offset = 8;
    if(!inlineresultproducer(&producer,&tuple))return 15;
    producer.d.offset = 4;
    if(inlineresultproducer(&producer,&tuple))return 16;
    producer.d.offset = 0;
    if(inlineresultproducer(&producer,&tuple))return 17;
    producer.op = IADDW;
    if(inlineresultproducer(&producer,&tuple))return 18;
    producer.mm = Afp;
    if(!inlineresultproducer(&producer,&tuple))return 19;
    frame.d.reg = 96;
    binding.op = ILEA; binding.sm = Afp; binding.s.reg = 32; binding.mm = Anone;
    if(!inlinebinding(&binding,&frame,&tuple,104,32,128))return 20;
    frame.d.reg = 36;
    if(inlinebinding(&binding,&frame,&tuple,104,32,128))return 30;
    frame.d.reg = 96; binding.s.reg = 80;
    if(inlinebinding(&binding,&frame,&tuple,104,32,128))return 21;
    binding.s.reg = 88;
    if(inlinebinding(&binding,&frame,&tuple,104,32,128))return 22;
    binding.s.reg = 36;
    if(inlinebinding(&binding,&frame,&tuple,104,32,128))return 23;
    binding.s.reg = 24;
    if(inlinebinding(&binding,&frame,&tuple,104,32,128))return 24;
    binding.s.reg = 16;
    if(inlinebinding(&binding,&frame,&tuple,104,32,128))return 25;
    binding.sm = Amp; binding.s.reg = 8;
    if(!inlinebinding(&binding,&frame,&tuple,104,32,32))return 26;
    if(inlinebinding(&binding,&frame,&tuple,104,32,31))return 27;
    binding.sm = Aimm;
    if(inlinebinding(&binding,&frame,&tuple,104,32,128))return 28;
    return 0;
}
'''.replace('DEFINES', defines).replace('PRODUCTION', production)
    controls = ((None, None, 0), ('a->kind != b->kind', '0', 14),
                ('(input->s.reg < frame->d.reg+IBY2WD && input->s.reg+width > frame->d.reg)', '0', 30))
    with tempfile.TemporaryDirectory(prefix='dis64-tuples-') as directory:
        work = Path(directory)
        for old, new, expected in controls:
            actual = fixture.replace(old, new) if old else fixture
            if old and actual == fixture:
                raise RuntimeError('Inactive tuple compiler control')
            target, binary = work/'test.c', work/'test'
            target.write_text(actual)
            command = ['cc', '-O2', '-g', '-fsanitize=undefined', '-fno-sanitize-recover=all']
            command += ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'include', 'limbo')]
            subprocess.run(command+[str(target), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)])
            if result.returncode != expected:
                raise RuntimeError('Invalid tuple geometry/control outcome: '+str(result.returncode))
    print('PASS production tuple compiler geometry: scalar fields, padding, limits, forwarding identity, caller bounds/alias; two active removal controls')


if __name__ == '__main__':
    main()
