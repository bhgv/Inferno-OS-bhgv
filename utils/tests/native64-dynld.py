#!/usr/bin/env python3
"""Exercise the complete owning dynamic-object loader with native64 architecture refusal."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Link real object parsing and error cleanup; libc/lib9 supply the unselected branches normally.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lib9',type=Path,required=True)
    parser.add_argument('--source',type=Path,default=ROOT/'libdynld/dynld-amd64.c')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='native64-dynld-',dir=ROOT) as directory:
        binary=Path(directory)/'run'
        command=['cc','-O2','-DNDEBUG','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-fno-strict-aliasing']
        command+=['-I'+str(ROOT/path) for path in ('Linux/amd64/include','Linux/386/include','include')]
        command+=[str(ROOT/'libdynld/dynld.c'),str(args.source),str(ROOT/'utils/tests/native64-dynld.c'),str(args.lib9),'-lm','-pthread','-o',str(binary)]
        subprocess.run(command,check=True)
        subprocess.run([str(binary)],check=True)


if __name__=='__main__':
    main()
