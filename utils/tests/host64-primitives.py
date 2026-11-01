#!/usr/bin/env python3
"""Execute the actual hosted native64 assembly and FP register implementation."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
HEADER='''#include <stdint.h>
typedef unsigned long ulong; // LP64 primitive ABI under test.
typedef unsigned int uint; // Exact 32-bit Lock and FP register storage.
#include "host64-fp.h"
void setfcr(ulong);
void setfsr(ulong);
ulong getfcr(void);
ulong getfsr(void);
'''


# The adapter declares only the library boundary; target instructions and FPU storage are production code.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch',choices=('amd64','arm64','riscv64'),default='amd64')
    parser.add_argument('--cc',default='cc')
    parser.add_argument('--runner',default='')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='host64-primitives-',dir=ROOT) as directory:
        work=Path(directory)
        (work/'lib9.h').write_text(HEADER)
        command=shlex.split(args.cc)+['-O2','-DNDEBUG','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-pie','-nostdlib','-static','-I'+str(work),'-I'+str(ROOT/'include')]
        if args.arch=='riscv64': command+=['-march=rv64gc','-mabi=lp64d']
        command+=[str(ROOT/'utils/tests/host64-primitives.c'),str(ROOT/'lib9/setfcr-native64.c'),str(ROOT/('emu/port/asm-'+args.arch+'.S')),str(ROOT/'utils/tests/host64-start.S'),'-Wl,-e,_start','-o',str(work/'run')]
        env=dict(os.environ,TMPDIR=str(work))
        subprocess.run(command,env=env,check=True)
        subprocess.run(shlex.split(args.runner)+[str(work/'run')],env=env,check=True)
        print('Executed:',args.arch,'native host' if not args.runner else 'through '+args.runner)


if __name__=='__main__':
    main()
