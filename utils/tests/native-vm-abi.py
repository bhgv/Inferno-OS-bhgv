#!/usr/bin/env python3
"""Report actual compiler/runtime layout; optionally reject an incompatible native VM ABI."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning source tree.
UNITS = {  # Separate actual headers because compiler and VM have distinct Inst/Type declarations.
    'runtime': r'''
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <stddef.h>
#include <stdio.h>


/* Measure the real host layout without altering WORD or pointer declarations. */
int
main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu\n", sizeof(void*), sizeof(WORD),
        sizeof(Frame), offsetof(Frame,lr), offsetof(Frame,fp),
        offsetof(Frame,mr), offsetof(Frame,t));
    return 0;
}
''',
    'compiler': r'''
#include "limbo.h"
#include <stdio.h>


/* Read the actual compiler constants rather than an invented native frame model. */
int
main(void)
{
    printf("%d %d %d %d %d\n", IBY2WD, STemp, RTemp, DTemp, MaxTemp);
    return 0;
}
''',
}


# Keep this as a report by default; require compatibility explicitly before a port release.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--require-compatible', action='store_true')
    args = parser.parse_args()
    values = {}
    with tempfile.TemporaryDirectory(prefix='native-vm-abi-') as directory:
        td = Path(directory)
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g'))
        includes = ['-I'+str(ROOT/p) for p in ('Linux/386/include', 'include', 'limbo')]
        for name, text in UNITS.items():
            source=td/(name+'.c');binary=td/name
            source.write_text(text)
            subprocess.run(compiler+flags+includes+[str(source), '-o', str(binary)], check=True)
            values[name]=list(map(int, subprocess.check_output([str(binary)], text=True).split()))
    pointer, word, frame, lr, fp, mr, typ = values['runtime']
    cell, stemp, rtemp, dtemp, maxtemp = values['compiler']
    print('runtime: pointer='+str(pointer)+' WORD='+str(word)+' Frame='+str(frame)+' links='+str([lr,fp,mr,typ]))
    print('compiler: cell='+str(cell)+' temporaries='+str([stemp,rtemp,dtemp,maxtemp]))
    failures = []
    if word != cell:
        failures.append('runtime WORD width differs from compiler cell width')
    if pointer != cell:
        failures.append('native pointers do not fit compiler reference cells')
    if frame > stemp:
        failures.append('native Frame header overlaps compiler temporaries')
    for failure in failures:
        print('BLOCKED: '+failure)
    if not failures:
        print('PASS basic native layout contracts; full loader/GC/runtime validation remains required')
    if args.require_compatible and failures:
        raise SystemExit(1)


# This probe never rewrites headers or changes the bytecode ABI.
if __name__ == '__main__':
    main()
