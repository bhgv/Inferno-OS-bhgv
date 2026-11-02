#!/usr/bin/env python3
"""Execute the production call-setup boundary resolver with actual compiler records."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Compile the actual resolver; removing its ownership check must expose the argument-store interior.
def main():
    source = (ROOT/'limbo/dis64-inline.c').read_text()
    begin = source.index('static Inst*\ninlinetarget(')
    body = source[begin:source.index('\n}', begin)+2]
    fixture = r'''
#include "limbo.h"
BODY


/* Ordinary PCs, frame entries and terminal continuations are allowed; setup interiors are not. */
int
main(void)
{
    Inst input[6], flat[8], *original[6];
    unsigned int mapping[5] = {1,2,3,4,5}, owners[6] = {0,1,1,1,4,5};
    int i;
    memset(input, 0, sizeof(input));
    for(i = 0; i < 6; i++)original[i] = &input[i];
    input[1].op = IFRAME;
    input[2].op = IMOVW;
    input[3].op = ICALL;
    input[5].op = IRET;
    if(inlinetarget(1,1,6,mapping,original,owners,flat) != &flat[1])return 1;
    if(inlinetarget(5,1,6,mapping,original,owners,flat) != &flat[5])return 2;
    if(inlinetarget(2,1,6,mapping,original,owners,flat) != nil)return 3;
    if(inlinetarget(3,1,6,mapping,original,owners,flat) != nil)return 4;
    if(inlinetarget(-1,1,6,mapping,original,owners,flat) != nil)return 5;
    if(inlinetarget(6,1,6,mapping,original,owners,flat) != nil)return 6;
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='dis64-setup-') as directory:
        work = Path(directory)
        for control in (False, True):
            actual = body.replace(' || owners[pc] != pc', '') if control else body
            if control and actual == body:
                raise RuntimeError('Inactive setup boundary control')
            target, binary = work/'test.c', work/'test'
            target.write_text(fixture.replace('BODY', actual))
            command = ['cc','-O2','-g','-fsanitize=undefined','-fno-sanitize-recover=all']
            command += ['-I'+str(ROOT/path) for path in ('Linux/386/include','include','limbo')]
            subprocess.run(command+[str(target),'-o',str(binary)],check=True)
            result = subprocess.run([str(binary)])
            if result.returncode != (3 if control else 0):
                raise RuntimeError('Invalid setup boundary outcome: '+str(result.returncode))
    print('PASS actual compiler setup resolver: frame/continuation, argument/CALL interiors, bounds; active interior-entry control')


if __name__ == '__main__':
    main()
