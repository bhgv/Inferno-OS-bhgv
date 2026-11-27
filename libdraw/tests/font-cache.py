#!/usr/bin/env python3
"""Exercise actual populated subfont-table growth and cache publication at target widths."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep real loadchar/resize bodies and public records, with explicit native-service boundaries.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('extent', 'geometry', 'publication', 'flush', 'capacity'))
    parser.add_argument('--char-model', choices=('native', 'signed', 'unsigned'), default='native')
    args = parser.parse_args()
    helper = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    fn = helper['function']
    font = (ROOT/'libdraw/font.c').read_text()
    source = re.search(r'^#define\s+PJW[^\n]*', font, re.M).group()+'\n'+fn(font, 'int\nloadchar(')+'\n'+fn(font, 'static int\nfontresize(')+'\n'+fn(font, 'int\ncachechars(')
    if args.control == 'extent':
        source = source.replace('memmove(f->subf, of, f->nsubf*sizeof *subf);', 'memmove(f->subf, of, (f->nsubf+DSUBF)*sizeof *subf);', 1)
    elif args.control == 'geometry':
        source = source.replace('i = fontresize(f, wid, f->ncache, depth);', 'f->width = wid; f->maxdepth = depth; i = fontresize(f, wid, f->ncache, depth);', 1)
    elif args.control == 'publication':
        source = source.replace('if(flushimage(f->display, 0) < 0)', "c->value = r;\n\tif(flushimage(f->display, 0) < 0)", 1)
    elif args.control == 'flush':
        source = source.replace('if(flushimage(d, 0) < 0){', 'if(flushimage(d, 0) < -1){', 1)
    source += '\n'+re.sub(r'^#include[^\n]*\n', '', (ROOT/'lib9/rune.c').read_text(), flags=re.M)
    if args.control == 'capacity':
        source = source.replace('if(wid < 0 || wid > 32767 || ncache <= NFLOOK || ncache > MAXFCACHE ||\n\t   (vlong)(ncache-1)*wid > 65535)', 'if(0)', 1)
    arith = (ROOT/'libdraw/arith.c').read_text()
    source += '\n'+'\n'.join(fn(arith, signature) for signature in ('Point\nPt(', 'Point\naddpt(', 'Rectangle\nRect('))
    text = (ROOT/'libdraw/tests/font-cache.c').read_text().replace('IMPLEMENTATION', source)
    text = helper['portfixture'](text, '#include "draw.h"')
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    guard = {'amd64': '!defined(__x86_64__)', 'arm64': '!defined(__aarch64__)', 'riscv64': '!defined(__riscv) || __riscv_xlen != 64'}[arch]
    text = '#if '+guard+'\n#error native target compiler mismatch\n#endif\n'+text
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    if args.char_model != 'native':
        flags += ['-f'+args.char_model+'-char']
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'include', 'emu/port')]
    with tempfile.TemporaryDirectory(prefix='font-cache-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        path = Path(directory)/'test.c'
        binary = Path(directory)/'test'
        path.write_text(text)
        subprocess.run(shlex.split(os.environ.get('CC', 'cc'))+flags+includes+[str(path), '-o', str(binary)], check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER', ''))+[str(binary)], capture_output=True, text=True, timeout=60)
        if args.control:
            stage = {'extent': 'source extent', 'geometry': 'resize rollback', 'publication': 'glyph publication', 'flush': 'resize rollback', 'capacity': 'cache capacity'}[args.control]
            if result.returncode != 10 or result.stderr.strip() != 'Font cache mismatch: '+stage:
                raise RuntimeError('control missed semantic stage: '+result.stdout+result.stderr)
            print('PASS populated font cache semantic control:', args.control, arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
