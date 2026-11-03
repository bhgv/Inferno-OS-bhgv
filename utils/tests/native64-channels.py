#!/usr/bin/env python3
"""Check real channel/String consumers and untruncated subfont paths with native Heap owners."""
import argparse
import json
import os
from pathlib import Path
import random
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Independently evaluate one protocol token using bounded Python integers.
def channel(text):
    token = re.split(r'[ \t\r\n]', text.lstrip(' \t\r\n'))[0]
    if not token or not re.fullmatch(r'(?:[rgbkamx][1-8]){1,4}', token):
        return 0
    value = 0
    for index in range(0, len(token), 2):
        value = value*256+'rgbkamx'.index(token[index])*16+int(token[index+1])
    return value if depth(value) else 0


# Derive depth/text from the protocol's finite field widths, independent of native ulong size.
def depth(value):
    if not 0 < value <= 0xffffffff:
        return 0
    count = 0
    while value:
        field, value = value & 255, value >> 8
        if field//16 >= 7 or not 1 <= field % 16 <= 8:
            return 0
        count += field % 16
    return count if (count >= 8 and count % 8 == 0) or (count < 8 and 8 % count == 0) else 0


# Keep valid and malformed token cases separate from arbitrary raw descriptor cases.
def rows():
    rng = random.Random(112)
    texts = ['', 'r8g8b8a8', 'r5g6b5', 'r3', 'r9', 'r0g8', 'r8g8b8a8x8', 'r8 '*4, ' \tr8\nignored', 'é']
    for _ in range(700):
        value = ''.join(rng.choice('rgbkamx')+str(rng.randrange(10)) for _ in range(rng.randrange(1, 7)))
        texts.append(value)
    values = [0, 1, 0x08182848, 0x68686868, 0x0808080808080808, 0x108182848, 0xffffffffffffffff]
    values += [rng.getrandbits(32) for _ in range(300)]
    return ('static ChannelCase channels[] = {\n'+',\n'.join('{'+json.dumps(t, ensure_ascii=False)+','+str(channel(t))+'UL}' for t in texts)+'\n};\n'
            'static DepthCase depths[] = {\n'+',\n'.join('{'+hex(v)+'ULL,'+str(depth(v))+'}' for v in values)+'\n};\n')


# Compile production Heap/String/channel bodies; only allocator capacity, error text and file probing are adapters.
def unit(control):
    support = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    fn = support['function']
    prefix = (ROOT/'libinterp/tests/dis64-runtime.c').read_text().split('IMPLEMENTATION')[0]
    prefix = '#include <stdarg.h>\n'+prefix
    prefix = prefix.replace('static void\nrequire(', 'static void trackedfree(void*); // Control-stop cleanup of fixture allocations.\nstatic void\nrequire(', 1)
    prefix = prefix.replace('abort();', 'for(int i=0;i<RuntimeBlocks;i++) if(blocks[i].pointer!=NULL) trackedfree(blocks[i].pointer);\n\t\texit(10);', 1)
    prefix = prefix.replace('Dis64 runtime check failed', 'Channel semantic mismatch', 1)
    prefix = prefix.replace('Type Tmodlink = {1, freemodlink, markheap, -1, 1, 0, 0, {0x80}};', 'Type Tmodlink = {1}; // Module execution is outside this fixture.', 1)
    prefix = prefix.replace('void\npoolfree(', 'static void observe(void*); // Result publication precedes displaced owner retirement.\nvoid\npoolfree(',1)
    prefix = prefix.replace('trackedfree((uchar*)pointer-O(Bhdr, u.data));','observe(pointer);\n\ttrackedfree((uchar*)pointer-O(Bhdr, u.data));',1)
    old = fn(prefix,'void\nkwerrstr(')
    prefix = prefix.replace(old, 'void\nkwerrstr(char *format,...)\n{\n va_list args; va_start(args,format); vsnprintf(message,sizeof(message),format,args); va_end(args);\n}',1)
    heap = (ROOT/'libinterp/heap.c').read_text()
    string = (ROOT/'libinterp/string.c').read_text()
    draw = (ROOT/'libinterp/draw.c').read_text()
    parts = [fn((ROOT/'emu/port/dis.c').read_text(),'int\nkprotect(')]
    parts += [fn(heap, s) for s in ('void\nfreeptrs(','void\nfreeheap(','void\ndestroy(','void\nfreetype(','void\nfreestring(','Heap*\nnheap(','int\nhmsize(')]
    parts += [fn(string, s) for s in ('String*\nnewstring(','String*\nnewrunes(','String*\nc2string(','char*\nstring2c(')]
    parts += [fn((ROOT/'libinterp/runt.c').read_text(),'void\nretstr('),fn(draw,'static void\ndrawcommitresult(')]
    consumer = '\n\n'.join(fn(draw,s) for s in ('void\nChans_text(','void\nChans_mk(','char*\nsubfontname('))
    for name in ('Chans_text','Chans_mk'):
        consumer = consumer.replace('F_'+name, 'F_Draw_'+name).replace('\n'+name+'(', '\nDraw_'+name+'(')
    native = re.sub(r'^#include[^\n]*\n','',(ROOT/'libdraw/chan.c').read_text(),flags=re.M)
    if control=='wide': native=native.replace('if((uvlong)c > 0xffffffffUL)\n\t\treturn 0;', 'if(0) return 0;',1)
    if control=='parse': native=native.replace('return chantodepth(c) == 0 ? 0 : c;', 'return c & 0xffffffffUL;',1)
    if control=='publication': parts[-1]=parts[-1].replace('memmove(slot, &value, sizeof(value));\n\tdestroy(old);','destroy(old);\n\tmemmove(slot, &value, sizeof(value));',1)
    if control=='path': consumer=consumer.replace("path[length+1] = '0'+i;", "path[length+1] = '9';",1)
    fixture = (ROOT/'utils/tests/native64-channels.c').read_text()
    source = prefix+'\n#include "draw.h"\n#include "draw-native64.h"\n'+fixture
    return source.replace('DATA_ROWS',rows()).replace('REAL_IMPLEMENTATION','\n\n'.join(parts)+'\n'+native+'\n'+consumer)


# Strict controls must hit explicit predicates; sanitizer errors, compiler errors and signals never count as success.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo',type=Path,required=True)
    args=parser.parse_args()
    env=dict(os.environ)
    arch=env.get('NATIVE64_ARCH','amd64')
    support=runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    command=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    if arch=='arm64': command+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
    elif arch=='riscv64': command+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
    runner=shlex.split(env.get('NATIVE64_RUNNER',''))
    with tempfile.TemporaryDirectory(prefix='native64-channels-',dir=ROOT) as directory:
        work=Path(directory)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()),'-H','-M','Draw','-I'+str(ROOT/'module'),str(ROOT/'module/draw-native64.m')]))
        includes=['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        for mode in ('WORD32','WORD32-file','wide','parse','publication','path'):
            source=work/(mode+'.c')
            source.write_text(support['portfixture'](unit(None if mode.startswith('WORD') else mode),'#include "vmfetch.h"'))
            flags=['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode.endswith('file') else [])
            binary=work/mode
            subprocess.run(command+flags+includes+['-ffunction-sections','-fdata-sections',str(source),str(ROOT/'lib9/rune.c'),'-Wl,--gc-sections','-Wl,--wrap=free','-lm','-o',str(binary)],check=True)
            result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=60)
            if mode.startswith('WORD'):
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(mode+': '+result.stdout,end='',flush=True)
            elif result.returncode!=10 or 'Channel semantic mismatch' not in result.stderr:
                raise RuntimeError('Channel control missed predicate '+mode+': '+str(result.returncode)+' '+result.stdout+result.stderr)
            else: print('PASS channel semantic control:',mode,arch,flush=True)


if __name__=='__main__':
    main()
