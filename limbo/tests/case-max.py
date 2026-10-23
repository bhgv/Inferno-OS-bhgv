#!/usr/bin/env python3
"""Run production case generation and data emission into real VM dispatch bodies."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
com = Path(os.environ.get('CASE_MAX_COM', root / 'limbo/com.c')).read_text()
dis = Path(os.environ.get('CASE_MAX_DIS', root / 'limbo/dis.c')).read_text()
asm = Path(os.environ.get('CASE_MAX_ASM', root / 'limbo/asm.c')).read_text()


# Extract complete functions without counting braces inside C strings/comments.
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


compiler = [function(com, s) for s in ('static int\ndogoto(', 'static void\nfillrange(',
                                      'static int\nnconstqual(', 'void\ncasecom(')]
# Keep the new wire-bound helper available when independently testing old codegen.
helper = com if 'Long\ncaseend(' in com else (root / 'limbo/com.c').read_text()
compiler.insert(0, function(helper, 'Long\ncaseend('))
compiler.append(function((root / 'limbo/gen.c').read_text(), 'void\npatch('))
compiler.append(function((root / 'limbo/typecheck.c').read_text(), 'int\nfindlab('))
prefix = dis[dis.index('static\tvoid\tdisbig'):dis.index('/* Reject values')]
data = [function(dis, s) for s in ('void\ndiscon(', 'void\ndisdata(', 'void\ndisword(',
                                  'void\ndisvar(', 'static void\ndisdatum(',
                                  'static void\ndisbyte(', 'static void\ndisbytes(',
                                  'static void\ndisint(', 'static void\ndisbig(',
                                  'static void\ndisreal(', 'static void\ndisstring(',
                                  'static void\ndisflush(')]
data.insert(0, function((root / 'limbo/dtocanon.c').read_text(), 'void\ndtocanon('))
data.append(function(asm, 'void\nasminitializer('))
xec = (root / 'libinterp/xec.c').read_text()
vm = [function(xec, 'OP('+name+')') for name in
      ('movw', 'movl', 'beqw', 'beql', 'bltw', 'bgtw', 'subw', 'jmp', 'icase', 'casel', 'igoto')]
unit = (root / 'limbo/tests/case-max.c').read_text()
unit = unit.replace('COMPILER_IMPLEMENTATION', '\n'.join(compiler))
unit = unit.replace('DATA_IMPLEMENTATION', prefix+'\n'+'\n'.join(data))
unit = unit.replace('VM_IMPLEMENTATION', '\n'.join(vm))
with tempfile.TemporaryDirectory(prefix='case-max-') as directory:
    target = Path(directory) / 'case-max.c'
    target.write_text(unit)
    binary = Path(directory) / 'case-max'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS',
                       '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    # Low mappings preserve the intentionally 32-bit VM pointer casts in this host slice.
    cmd += ['-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast']
    cmd += ['-I' + str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(cmd + [str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)] + shlex.split(os.environ.get('CASE_MAX_CASE', '')),
                   check=True, timeout=60)
