#!/usr/bin/env python3
"""Execute both startup option blocks with production shell tokenization."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, signature):
    """Extract a complete function while ignoring braces inside comments and literals."""
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


source = (root / 'lib9/tokenize.c').read_text()
parts = [function(source, 'static char*\nqtoken('), function(source, 'int\ntokenize(')]
parts[-1] = parts[-1].replace('tokenize(', 'realtokenize(', 1)
for name, path in [('portable', 'emu/port/main.c'), ('android', 'emu/Android/amain.c')]:
    source = Path(os.environ.get(name.upper()+'_SOURCE', root / path)).read_text()
    start = source.index('enva[0] = "emu";')
    end = source.index('option(envc, enva, envusage);', start)+len('option(envc, enva, envusage);')
    parts.append('static int '+name+'(char *opt) { char *enva[20]; int envc;\n'+
                 source[start:end]+'\nreturn envc; }')
unit = (root / 'emu/port/tests/startup-options.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='startup-options-') as directory:
    td = Path(directory)
    target = td / 'options.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    binary = td / 'options'
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
