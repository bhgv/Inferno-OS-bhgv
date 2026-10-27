#!/usr/bin/env python3
"""Exercise production optimizer byte ranges before use/def table publication."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning compiler source tree.
CURRENT = (ROOT/'limbo/optim.c').read_text()  # Supplies new helpers for parent replay.
SOURCE = Path(os.environ.get('OPTIM_RANGES_SOURCE',ROOT/'limbo/optim.c')).read_text()


# Extract one complete C definition without counting comment or literal braces.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{',start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{': depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0: return source[start:brace+token.end()]
    raise RuntimeError(signature)


parts = []
for name in ('optsize','imm','desc','fpoff','size','optdeclend','finddec'):
    signature = 'static int\n'+name+'('
    parts.append(function(SOURCE if signature in SOURCE else CURRENT,signature))
unit = (ROOT/'limbo/tests/optimizer-ranges.c').read_text().replace('IMPLEMENTATION','\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='optimizer-ranges-') as temporary:
    td = Path(temporary)
    source = td/'ranges.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (4,8):
        model = td/str(width)
        model.mkdir()
        isa = re.sub(r'IBY2WD\s*=\s*4','IBY2WD = '+str(width),(ROOT/'include/isa.h').read_text(),count=1)
        (model/'isa.h').write_text(isa)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','include','limbo')]
        binary = model/'ranges'
        subprocess.run(command+includes+[str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=60)
