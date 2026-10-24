#!/usr/bin/env python3
"""Check real emu/os heap queries, native slot widths and bounded pagination."""
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
    source = Path(os.environ.get('HEAP_QUERY_SOURCE', ROOT/variant/'port/devprog.c')).read_text()
    signatures = ('static int\nprogheap(',)
    parts = [function((ROOT/'libinterp/string.c').read_text(), 'char*\nstring2c(')]+[function(source, name) for name in signatures]
    unit = (ROOT/'libinterp/tests/heap-query.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
    with tempfile.TemporaryDirectory(prefix='heap-query-') as directory:
        td=Path(directory);target=td/'query.c';target.write_text(unit)
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        for bits in (64,32):
            model=td/str(bits);model.mkdir();header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=td/('query-'+str(bits))
            formatter=[str(ROOT/('lib9/'+name+'.c')) for name in ('rune','fmtprint','sprint','fltfmt','pow10','isnan-posix','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen')]
            subprocess.run(command+includes+[str(target)]+formatter+['-lm','-o',str(binary)],check=True)
            print(variant,bits,flush=True)
            subprocess.run([str(binary)]+shlex.split(os.environ.get('HEAP_QUERY_CASE','')),check=True)
