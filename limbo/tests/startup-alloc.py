#!/usr/bin/env python3
"""Check production startup path assembly and implementation-module allocation failures."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract complete production functions independently of generated parser tables."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


types = Path(os.environ.get('STARTUP_TYPES', root / 'limbo/types.c')).read_text()
alloc = function((root / 'limbo/lex.c').read_text(), 'void*\nallocmem(')
bodies = []
with tempfile.TemporaryDirectory(prefix='startup-alloc-') as directory:
    td = Path(directory)
    for parser in ('limbo.y', 'y.tab.c'):
        source = Path(os.environ.get('STARTUP_DRIVER', root / 'limbo' / parser)).read_text()
        if 'static void\nrootincludes(' in source:
            body = function(source, 'static void\nrootincludes(')
            assert '\trootincludes();' in function(source, 'void\nmain(')
        else:
            # Exercise the original production startup block with the same adapters.
            start = source.index('\tif((root = getroot()) != nil){')
            end = source.index('\n\n\tif(argc == 0)', start)
            body = 'static void\nrootincludes(void)\n{\nchar *root;\n'+source[start:end]+'\n}\n'
        bodies.append(body)
        parts = alloc+'\n'+body+'\n'+function(types, 'void\nmoddecled(')
        target = td / (parser+'.c')
        target.write_text((root / 'limbo/tests/startup-alloc.c').read_text().replace('IMPLEMENTATION', parts))
        binary = td / (parser+'.test')
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+os.environ.get('STARTUP_CASE', '').split(), check=True, timeout=20)
    assert bodies[0] == bodies[1], 'grammar and generated parser startup helpers differ'
