#!/usr/bin/env python3
"""Check actual emu/os process diagnostics with real formatters and frame layouts."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract production functions without interpreting braces in comments/strings."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:return source[start:brace+token.end()]
    raise ValueError(signature)


for variant in ('emu','os'):
    source = Path(os.environ.get('PROG_DIAGNOSTIC_SOURCE', ROOT/variant/'port/devprog.c')).read_text()
    signatures = ('static int\nprogsize(', 'static int\nprogstack(', 'static int\ncalldepth(', 'WORD\nmodstatus(')
    parts = [function(source, name) for name in signatures]
    unit = (ROOT/'libinterp/tests/prog-diagnostics.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
    with tempfile.TemporaryDirectory(prefix='prog-diagnostics-') as directory:
        td=Path(directory);target=td/'diagnostics.c';target.write_text(unit)
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        for bits in (64,32):
            model=td/str(bits);model.mkdir();header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=td/('diagnostics-'+str(bits))
            formatter=[str(ROOT/('lib9/'+name+'.c')) for name in ('rune','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen')]
            subprocess.run(command+includes+[str(target)]+formatter+['-o',str(binary)],check=True)
            print(variant,bits,flush=True)
            subprocess.run([str(binary)]+shlex.split(os.environ.get('PROG_DIAGNOSTIC_CASE','')),check=True)
