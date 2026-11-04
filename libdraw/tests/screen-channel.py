#!/usr/bin/env python3
"""Execute actual native Screen constructors and window publication against public headers."""
import argparse
import os
import re
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract production bodies, replacing only native allocation, transport and image-constructor services.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('owned', 'public', 'channel', 'publication', 'attachment', 'validation'))
    args = parser.parse_args()
    fn = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    source = (ROOT/'libdraw/window.c').read_text()
    signatures = ('Screen*\nallocscreen(', 'Screen*\npublicscreen(', 'Image*\n_allocwindow(', 'Image*\nallocwindow(')
    bodies = 'static int screenid; // Actual native screen ID allocator.\n'+'\n\n'.join(fn(source,s) for s in signatures)
    mutations = {
        'owned': ('s->chan = image->chan;', 's->chan = 0;'),
        'public': ('s->chan = chan;', 's->chan = 0;'),
        'channel': ('r, s->chan, 0, val', 'r, CMAP8, 0, val'),
        'publication': ('if(i == 0)\n\t\treturn 0;', 'if(i == 0) { d->windows = nil; return 0; }'),
        'attachment': ('if(i)\n\t\tattachtowindow(i);', 'if(0)\n\t\tattachtowindow(i);'),
        'validation': ('id <= 0 || chan == 0', 'id < 0 || chan == 0'),
    }
    if args.control:
        old,new = mutations[args.control]
        if bodies.count(old) != 1:
            raise RuntimeError('screen control marker drift')
        bodies = bodies.replace(old,new,1)
    text = (ROOT/'libdraw/tests/screen-channel.c').read_text().replace('SCREEN_IMPLEMENTATION',bodies)
    text += '\n'+re.sub(r'^#include[^\n]*\n','',(ROOT/'libdraw/chan.c').read_text(),flags=re.MULTILINE)
    text += '\n'+fn((ROOT/'libdraw/arith.c').read_text(),'Rectangle\nRect(')
    arch = os.environ.get('NATIVE64_ARCH','amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','include')]
    with tempfile.TemporaryDirectory(prefix='screen-channel-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        path = Path(directory)/'test.c';binary = Path(directory)/'test'
        path.write_text(text)
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+includes+[str(path),'-o',str(binary)],check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=60)
        if args.control:
            stage = {'owned':'retained channel','public':'retained channel','channel':'window channel','publication':'window refusal','attachment':'attachment count','validation':'invalid public screen'}[args.control]
            if result.returncode != 10 or result.stderr.strip() != 'Screen channel mismatch: '+stage:
                raise RuntimeError('screen control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
            print('PASS native Screen control:',args.control,arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout,end='')


if __name__ == '__main__':
    main()
