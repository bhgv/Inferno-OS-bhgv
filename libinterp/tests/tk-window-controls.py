#!/usr/bin/env python3
"""Require old singleton-window and borrowed-display behaviors to fail the actual ownership fixture."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Compile real changed functions with each old fault restored; a compiler error is not an active control.
def main():
    top = (ROOT/'libinterp/tk.c').read_text()
    at = top.index('\tr = *f->ret;', top.index('void\nTk_toplevel('))
    reuse = '''\tt = currun()->tktop;
\tif(t != nil){
\t\th = D2H(t);
\t\th->ref++;
\t\tpoolmutable(h);
\t\tr = *f->ret;
\t\t*f->ret = (Tk_Toplevel*)t;
\t\tdestroy(r);
\t\treturn;
\t}
'''
    draw = (ROOT/'libinterp/draw.c').read_text()
    if '\theap->ref++;' not in draw:
        raise RuntimeError('Missing display ownership witness')
    with tempfile.TemporaryDirectory(prefix='tk-window-controls-') as temporary:
        work = Path(temporary)
        controls = (
            ('singleton-top', 'TK_OWNER_SOURCE', top[:at]+reuse+top[at:]),
            ('borrowed-display', 'TK_OWNER_DRAW_SOURCE', draw.replace('\theap->ref++;', '\t/* Removed retain. */', 1)),
        )
        for name, variable, source in controls:
            path = work/(name+'.c')
            path.write_text(source)
            environment = dict(os.environ, TK_OWNER_CASE='top', TK_OWNER_MODELS='32')
            environment[variable] = str(path)
            result = subprocess.run([sys.executable, str(ROOT/'libinterp/tests/tk-owners.py')],
                                    env=environment, capture_output=True, timeout=60)
            output = result.stdout+result.stderr
            if result.returncode == 0 or b'Tk owner check failed' not in output:
                raise RuntimeError('Inactive window control '+name+'\n'+output.decode(errors='replace'))
            print('PASS active WORD32 window control:', name)


if __name__ == '__main__':
    main()
