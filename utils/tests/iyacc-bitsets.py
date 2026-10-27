#!/usr/bin/env python3
"""Exercise the actual iyacc terminal bit macros at every 32-bit boundary."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning repository.
source = Path(os.environ.get('IYACC_SOURCE', ROOT/'utils/iyacc/yacc.c')).read_text()
macros = '\n'.join(re.findall(r'^#define (?:BIT|SETBIT)\([^\n]+', source, re.M))
fixture = r'''
#include <stdio.h>
#include <stdlib.h>


/* Set/test each terminal, then clear it while preserving all other positions. */
int
main(void)
{
    unsigned int bits[32]={0};
    int i, j;
    for(i=0;i<1024;i++){
        SETBIT(bits,i);
        for(j=0;j<1024;j++)
            if((BIT(bits,j)!=0)!=(j<=i))return 1;
    }
    for(i=0;i<1024;i++){
        bits[i>>5]&=~(1U<<(i&31));
        for(j=0;j<1024;j++)
            if((BIT(bits,j)!=0)!=(j>i))return 2;
    }
    puts("PASS iyacc bitsets: 2097152 exact membership checks across 32 terminal words");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='iyacc-bitsets-') as directory:
    path=Path(directory)/'bits.c';binary=Path(directory)/'bits'
    path.write_text(macros+'\n'+fixture)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(command+[str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
