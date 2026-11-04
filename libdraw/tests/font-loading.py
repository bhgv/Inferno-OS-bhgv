#!/usr/bin/env python3
"""Run actual font parser/file loader, active numeric oracles and allocation/I-O refusals."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Preserve production bodies and isolate diagnostic omissions from compiler/sanitizer failures.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('range', 'separator', 'unicode', 'offset'))
    args = parser.parse_args()
    parts = []
    for name in ('buildfont', 'openfont'):
        path = Path(os.environ.get(name.upper()+'_SOURCE', ROOT/'libdraw'/(name+'.c')))
        parts.append(re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M))
    if args.control:
        old, new = {
            'range': ('if(value > (limit-digit)/base)', 'if(0 && value > (limit-digit)/base)'),
            'separator': ("digits == 0 || (*cursor != 0 && !fontspace((uchar)*cursor))", 'digits == 0'),
            'unicode': ('min > Runemax || max > Runemax', 'min >= Runemax || max >= Runemax'),
            'offset': ('if(numeric < 0)', 'if(0 && numeric < 0)'),
        }[args.control]
        if parts[0].count(old) != 1:
            raise RuntimeError('font control source drift')
        parts[0] = parts[0].replace(old, new, 1)
    unit = (ROOT/'libdraw/tests/font-loading.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
    oracle = runpy.run_path(str(ROOT/'libdraw/tests/font-oracles.py'))
    unit = unit.replace('FONT_ORACLES', oracle['rows']()+oracle['corpus_rows'](ROOT))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    unit = support['portfixture'](unit, '#include "draw.h"')
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    if arch == 'amd64':
        unit = '#ifndef __x86_64__\n#error x64 gate requires an x86-64 compiler\n#endif\n'+unit
    if arch == 'arm64':
        unit = '#ifndef __aarch64__\n#error ARM64 gate requires an AArch64 compiler\n#endif\n'+unit
    if arch == 'riscv64':
        unit = '#if !defined(__riscv) || __riscv_xlen != 64\n#error RV64 compiler required\n#endif\n'+unit
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', os.environ.get('CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all')))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'include', 'emu/port')]
    runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='font-loading-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        target = Path(directory)/'test.c'
        target.write_text(unit)
        binary = Path(directory)/'test'
        subprocess.run(cmd+flags+includes+[str(target), '-o', str(binary)], check=True)
        result = subprocess.run(runner+[str(binary)], capture_output=True, text=True, timeout=120)
        if args.control:
            stage = 'number status' if args.control in ('range', 'separator') else 'font admission'
            if result.returncode != 10 or ('Font input semantic mismatch: '+stage+' ') not in result.stderr:
                raise RuntimeError('font omission missed semantic predicate: '+result.stdout+result.stderr)
            print('PASS font semantic omission:', args.control, arch)
        else:
            if result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            print(result.stdout, end='')
    if not args.control:
        subprocess.run(cmd+includes+['-fsyntax-only', '-Werror=implicit-function-declaration', '-Werror=int-conversion']+
                       [str(ROOT/'libdraw'/p) for p in ('buildfont.c', 'openfont.c', 'getsubfont_ft.c', 'getsubfont.c', 'font.c')], check=True)


if __name__ == '__main__':
    main()
