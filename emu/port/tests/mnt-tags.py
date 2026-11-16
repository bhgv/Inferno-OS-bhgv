# Copyright (c) 2026 bhgv
# EN: Exercise exact production mount-tag functions at bitmap and wire-space boundaries.
# RU: Проверять настоящие mount-tag функции на границах bitmap и пространства wire-тегов.
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import argparse


# EN/RU: Compile the actual allocator bodies with their actual bitmap declaration.
# Компилировать исходные allocator-функции с исходным объявлением bitmap.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--signed-shift-control', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    source = (root / 'emu/port/devmnt.c').read_text()
    enum = re.search(r'enum\s*\{\s*TAGSHIFT.*?\};', source, re.S).group()
    member = re.search(r'\w+\s+tagmask\[NMASK\];', source).group()
    functions = source[source.index('static int\nalloctag(void)'):source.index('\nMntrpc*\nmntralloc(')]
    if args.signed_shift_control:
        functions = functions.replace('1U<<', '1<<')
    text = '#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\ntypedef uint32_t u32int;typedef unsigned long ulong;\n#define NOTAG 65535\n'
    text += enum + '\nstruct { ' + member + ' } mntalloc;\n' + functions
    text += '''
static void check(int valid) { if(!valid) { fputs("mount tag mismatch\\n",stderr); exit(2); } }
int main(void) {
    int i, round;
    for(round=0;round<2;round++) {
        mntalloc.tagmask[0]=1;
        mntalloc.tagmask[NMASK-1]=0x80000000U;
        for(i=1;i<NOTAG;i++) check(alloctag()==i);
        check(alloctag()==NOTAG);
        for(i=2;i<NOTAG;i+=2) freetag(i);
        for(i=2;i<NOTAG;i+=2) check(alloctag()==i);
        check(alloctag()==NOTAG);
        for(i=1;i<NOTAG;i++) freetag(i);
    }
    puts("PASS all 65534 mount tags: exhaustion, boundary bits, reuse, reserved tags");
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='mnt-tags-') as name:
        work = Path(name)
        (work / 'test.c').write_text(text)
        subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-O1', '-g', '-fsanitize=undefined',
            '-fno-sanitize-recover=all', str(work / 'test.c'), '-o', str(work / 'test')], check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER', '')) + [str(work / 'test')], capture_output=True, text=True)
        if args.signed_shift_control:
            if result.returncode == 0 or 'left shift of 1 by 31' not in result.stderr:
                raise RuntimeError('signed-shift control was not detected: ' + result.stderr)
            print('PASS mount tag signed-shift control')
        elif result.returncode:
            raise RuntimeError(result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
