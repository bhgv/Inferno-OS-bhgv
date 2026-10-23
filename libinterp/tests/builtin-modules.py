#!/usr/bin/env python3
"""Exercise real native-module constructors, unload and dynamic-loader ownership."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


load = Path(os.environ.get('BUILTIN_LOAD', root / 'libinterp/load.c')).read_text()
runt = Path(os.environ.get('BUILTIN_RUNT', root / 'libinterp/runt.c')).read_text()
link = (root / 'libinterp/link.c').read_text()
heap = (root / 'libinterp/heap.c').read_text()
parts = [function((root / 'emu/port/dis.c').read_text(), 'int\nkprotect(')]
parts += [function(heap, s) for s in ('Type*\ndtype(', 'void\nfreetype(')]
parts += [function(load, s) for s in ('Module*\nnewmod(', 'Module*\nlookmod(', 'void\nfreemod(', 'void\nunload(')]
parts += [function(link, s) for s in ('static void\nnewlink(', 'void\nruntime(', 'void\ndestroylinks(')]
if 'typedef struct BuiltinModule' in runt:
    parts.append(runt[runt.index('typedef struct BuiltinModule'):runt.index('/* Validate table bounds')])
# Previous source used a spaced Module pointer return spelling.
sig = 'Module*\nbuiltinmod(' if 'Module*\nbuiltinmod(' in runt else 'Module *\nbuiltinmod('
parts.append(function(runt, sig))
with tempfile.TemporaryDirectory(prefix='builtin-modules-') as directory:
    for platform in ('Plan9', 'Inferno'):
        dynamic = Path(os.environ.get('BUILTIN_DYNAMIC', root / 'libinterp' / ('dlm-'+platform+'.c'))).read_text()
        functions = [function(dynamic, s) for s in ('static void*\naddr(', 'Module*\nnewdyncode(', 'void\nfreedyncode(')]
        unit = (root / 'libinterp/tests/builtin-modules.c').read_text()
        # Reuse the compiler-generated Runtab ABI without linking its Sys table.
        unit = unit.replace('RUNTAB_TYPE', (root / 'libinterp/sysmod.h').read_text().splitlines()[0])
        unit = unit.replace('IMPLEMENTATION', '\n'.join(parts+functions))
        target = Path(directory) / (platform+'.c')
        target.write_text(unit)
        binary = Path(directory) / platform
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=60)
