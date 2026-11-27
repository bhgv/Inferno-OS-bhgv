#!/usr/bin/env python3
"""Exercise actual native display init/close with every staged ownership refusal."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep actual constructor/close, metadata/channel parsing and geometry; inject only native services.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('memory','channel','image','lock','descriptor','path','diagnostic','complete'))
    args = parser.parse_args()
    helper = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    fn = helper['function']
    init = (ROOT/'libdraw/init.c').read_text()
    capacity = next(line for line in init.splitlines() if line.startswith('#define\tNINFO'))
    locking = re.search(r'CHECKLOCKING\s*=\s*(\d+)',init).group(1)
    source = capacity+'\n#define CHECKLOCKING '+locking+' // Actual configured native locking policy.\n'
    source += (ROOT/'libdraw/drawinfo.h').read_text()+'\n'
    source += fn(init,'Display*\ninitdisplay(')+'\n'+fn(init,'static void\ndiscardrefresh(')+'\n'+fn(init,'void\nclosedisplay(')
    if args.control == 'memory':
        source = source.replace('free(disp);','/* omitted partial display retirement */',1)
    elif args.control == 'channel':
        source = source.replace('libchanclose(disp->datachan);','/* omitted retained channel retirement */',1)
    elif args.control == 'image':
        source = source.replace('freeimage(disp->opaque);','/* omitted partial default image retirement */',1)
    elif args.control == 'lock':
        source = source.replace('if(locked)\n\t\t\tlibqunlock(q);','/* omitted partial initial unlock */',1)
    elif args.control == 'descriptor':
        source = source.replace('if(datafd >= 0)\n\t\tlibclose(datafd);','/* omitted original data descriptor retirement */',1)
    elif args.control == 'path':
        source = source.replace('free(devdir);','/* omitted owned device path retirement */',1)
    elif args.control == 'diagnostic':
        source = source.replace('kwerrstr("initdisplay: %s", reason[0] != 0 ? reason : "native initialization failed");','USED(reason);',1)
    elif args.control == 'complete':
        source = source.replace('if(disp->white == nil)\n\t\tgoto Refuse;','/* omitted missing default refusal */',1)
    source += '\n'+re.sub(r'^#include[^\n]*\n','',(ROOT/'libdraw/chan.c').read_text(),flags=re.M)
    source += '\n'+fn((ROOT/'libdraw/arith.c').read_text(),'Rectangle\nRect(')
    text = helper['portfixture']((ROOT/'libdraw/tests/display-init.c').read_text().replace('IMPLEMENTATION',source),'#include "lib9.h"')
    arch = os.environ.get('NATIVE64_ARCH','amd64')
    guard = {'amd64':'!defined(__x86_64__)', 'arm64':'!defined(__aarch64__)', 'riscv64':'!defined(__riscv) || __riscv_xlen != 64'}[arch]
    text = '#if '+guard+'\n#error native target compiler mismatch\n#endif\n'+text
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','include','emu/port')]
    with tempfile.TemporaryDirectory(prefix='display-init-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        path = Path(directory)/'test.c';binary = Path(directory)/'test'
        path.write_text(text)
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+includes+[str(path),'-o',str(binary)],check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=60)
        if args.control:
            stage = {'memory':'memory baseline','channel':'channel baseline','image':'image baseline','lock':'lock release',
                     'descriptor':'descriptor baseline','path':'memory baseline','diagnostic':'diagnostic preservation','complete':'complete defaults'}[args.control]
            if result.returncode != 10 or result.stderr.strip() != 'Display init mismatch: '+stage:
                raise RuntimeError('control missed exact predicate: '+result.stdout+result.stderr)
            print('PASS native display-init semantic control:',args.control,arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout,end='')


if __name__ == '__main__':
    main()
