#!/usr/bin/env python3
"""Check actual subfont placement/measurement with wide reference coordinates."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Compile the actual Draw declarations, UTF decoder and geometry bodies at target width.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('metrics', 'placement', 'width'))
    parser.add_argument('--char-model', choices=('native', 'signed', 'unsigned'), default='native')
    args = parser.parse_args()
    helpers = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    source = re.sub(r'^#include[^\n]*\n', '', (ROOT/'libdraw/stringsubfont.c').read_text(), flags=re.M)
    if args.control == 'metrics':
        source = source.replace('return i->x <= i[1].x && i->top <= i->bottom && i->bottom <= f->height;', 'return 1;', 1)
    elif args.control == 'placement':
        source = source.replace('return n >= (-2147483647LL-1) && n <= 2147483647LL;', 'return 1;', 1)
    elif args.control == 'width':
        # Return the wrong but representable width; UB is never a successful control.
        source = source.replace('return p;\n\t\tp.x += f->info[c].width;', 'return Pt(0, p.y);\n\t\tp.x += f->info[c].width;', 1)
    rune = re.sub(r'^#include[^\n]*\n', '', (ROOT/'lib9/rune.c').read_text(), flags=re.M)
    arith = (ROOT/'libdraw/arith.c').read_text()
    implementation = source+'\n'+rune+'\n'+helpers['function'](arith, 'Point\nPt(')+'\n'+helpers['function'](arith, 'Rectangle\nRect(')
    text = (ROOT/'libdraw/tests/subfont-geometry.c').read_text().replace('IMPLEMENTATION', implementation)
    text = helpers['portfixture'](text, '#include "draw.h"')
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    guard = {'amd64': '!defined(__x86_64__)', 'arm64': '!defined(__aarch64__)', 'riscv64': '!defined(__riscv) || __riscv_xlen != 64'}[arch]
    text = '#if '+guard+'\n#error native target compiler mismatch\n#endif\n'+text
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    if args.char_model != 'native':
        flags += ['-f'+args.char_model+'-char']
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'include', 'emu/port')]
    with tempfile.TemporaryDirectory(prefix='subfont-geometry-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        path = Path(directory)/'test.c'
        binary = Path(directory)/'test'
        path.write_text(text)
        subprocess.run(shlex.split(os.environ.get('CC', 'cc'))+flags+includes+[str(path), '-o', str(binary)], check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER', ''))+[str(binary)], capture_output=True, text=True, timeout=60)
        if args.control:
            stage = {'metrics': 'metric refusal', 'placement': 'placement admission', 'width': 'width boundary'}[args.control]
            if result.returncode != 10 or result.stderr.strip() != 'Subfont geometry mismatch: '+stage:
                raise RuntimeError('control missed semantic stage: '+result.stdout+result.stderr)
            print('PASS subfont geometry semantic control:', args.control, arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
