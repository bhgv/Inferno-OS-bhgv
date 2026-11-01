#!/usr/bin/env python3
"""Check incremental classic-mk invalidation of the three actual caller-guard owners."""
from pathlib import Path
import argparse
import os
import subprocess


# Advance one contract's timestamp without changing bytes, and restore it even if mk fails.
def check(root, mk, header):
    path = root/'include'/header
    previous = path.stat()
    units = ((root/'libinterp', ('stack.o', 'xec.o'), ('stack.c', 'xec.c')),
             (root/'emu/Linux', ('dis.o',), ('../port/dis.c',)))
    # A preexisting rebuild would make the changed-header oracle vacuous.
    for directory, names, sources in units:
        result = subprocess.run([str(mk), '-n', *names], cwd=directory, capture_output=True, text=True, check=True)
        if any('-c' in line.split() and source in line.split() for line in result.stdout.splitlines() for source in sources):
            raise RuntimeError('Build the owning objects before testing dependencies: '+str(directory))
    newest = max((directory/name).stat().st_mtime_ns for directory, names, _ in units for name in names)
    try:
        os.utime(path, ns=(previous.st_atime_ns, max(newest, previous.st_mtime_ns)+2_000_000_000))
        for directory, names, sources in units:
            result = subprocess.run([str(mk), '-n', *names], cwd=directory, capture_output=True, text=True, check=True)
            lines = result.stdout.splitlines()
            for source in sources:
                if not any('-c' in line.split() and source in line.split() for line in lines):
                    raise RuntimeError(header+' failed to invalidate '+source+'\n'+result.stdout)
    finally:
        os.utime(path, ns=(previous.st_atime_ns, previous.st_mtime_ns))
    print('PASS actual mk dependency:', header, '-> stack/xec/dis')


# Require already built Linux objects; this test executes only mk dry runs.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--mk', type=Path, required=True)
    args = parser.parse_args()
    for header in ('native64-caller.h', 'native64-frame-cell.h', 'native64-model.h', 'dis-layout.h'):
        check(args.root.resolve(), args.mk.resolve(), header)


if __name__ == '__main__':
    main()
