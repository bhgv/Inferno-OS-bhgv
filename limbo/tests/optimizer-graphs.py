#!/usr/bin/env python3
"""Check production optimizer graph order, bounded C-stack use and wide counts."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning compiler tree.
SOURCE = Path(os.environ.get('OPTIM_GRAPH_SOURCE', ROOT/'limbo/optim.c')).read_text()


# Extract complete definitions without counting comment or literal braces.
def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', SOURCE[brace:]):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0: return SOURCE[start:brace+token.end()]
    raise RuntimeError(signature)


parts = ['typedef struct Bits Bits;\ntypedef struct Blist Blist;\ntypedef struct Block Block;',
         SOURCE[SOURCE.index('struct Bits\n'):SOURCE.index('struct Idlist\n')]]
if 'static void\noptwalk(' in SOURCE:
    parts += [SOURCE[SOURCE.index('typedef struct OptWalk'):SOURCE.index('static void\noptwalk(')],
              function('static void\noptwalk(')]
parts += [function('static void\n'+name+'(') for name in ('dfs0','loop0')]
parts += [function('static int\nbitc(')]
for name in ('bitcnt','bitcount'):
    result = 'uvlong' if 'static uvlong\n'+name+'(' in SOURCE else 'int'
    parts.append(function('static '+result+'\n'+name+'('))
unit = (ROOT/'limbo/tests/optimizer-graphs.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='optimizer-graphs-') as temporary:
    td = Path(temporary)
    source = td/'graphs.c'; source.write_text(unit)
    binary = td/'graphs'
    command = shlex.split(os.environ.get('CC','cc'))
    command += shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/arm/include','include','limbo')]
    subprocess.run(command+includes+[str(source),'-o',str(binary)],check=True)
    families = [os.environ['OPTIM_GRAPH_CASE']] if os.environ.get('OPTIM_GRAPH_CASE') else ['small','dfs','loop','count']
    for family in families:
        subprocess.run([str(binary)],env=dict(os.environ,OPTIM_GRAPH_CASE=family),check=True)
