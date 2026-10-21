#!/usr/bin/env python3
"""Check the proposed ABI contract; no active VM ABI is changed by this test."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=r'''
#include <assert.h>
#include <string.h>
#include "dis-layout.h"
int main(void) {
    DisAbi a;
    DisLayout l, saved;
    unsigned int offset, size, ptrbytes, i;
    unsigned char map[2];
    for(ptrbytes=4;ptrbytes<=8;ptrbytes+=4) {
        assert(disabinit(&a,ptrbytes));
        l.next=0; l.align=1;
        assert(disabfield(&a,&l,DisWord,&offset) && offset==0);
        assert(disabfield(&a,&l,DisPointer,&offset) && offset==ptrbytes);
        assert(disabfield(&a,&l,DisWord,&offset) && offset==ptrbytes*2);
        assert(disabfinish(&l,&size) && size==ptrbytes*3);
        map[0]=ptrbytes==8 ? 0x20 : 0x40;
        assert(disabmap(&a,size,map,1));
        assert(!disabmap(&a,ptrbytes,map,1)); /* pointer extends past object */
        if(ptrbytes==8) {
            map[0]=0x40; assert(!disabmap(&a,size,map,1)); /* misaligned pointer */
            map[0]=0x30; assert(!disabmap(&a,size,map,1)); /* marked second half */
        }
        assert(disabframe(&a)==(ptrbytes==8 ? 64 : 32));
        l.next=0; l.align=1;
        for(i=0;i<100;i++) assert(disabfield(&a,&l,DisWord,&offset) && offset==i*4);
        assert(disabfinish(&l,&size) && size==400); /* int never becomes 64-bit */
        l.next=DisMaxOffset-1; l.align=1; saved=l; offset=123;
        assert(!disabfield(&a,&l,DisPointer,&offset));
        assert(memcmp(&l,&saved,sizeof(l))==0 && offset==123); /* failure is atomic */
        l.next=4; l.align=3; assert(!disabfinish(&l,&size));
        assert(disabmap(&a,0,0,0));
        assert(!disabmap(&a,0,map,1));
        assert(!disabmap(&a,32,0,1));
    }
    assert(!disabinit(&a,3));
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='dis-layout-') as td:
    td=Path(td); src=td/'layout.c'; src.write_text(source)
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+
                   shlex.split(os.environ.get('CFLAGS','-O1 -g'))+
                   ['-I'+str(root/'include'),str(src),'-o',str(td/'layout')],check=True)
    subprocess.run([str(td/'layout')],check=True)
print('PASS proposed Dis ABI32/ABI64: scalar/pointer separation, layouts, frame slots, GC maps and overflow rejection')
