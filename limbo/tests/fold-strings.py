#!/usr/bin/env python3
"""Compare actual constant string casts and VM conversions against Python decimal oracles."""
import os
from pathlib import Path
import random
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('NODES_SOURCE', root / 'limbo/nodes.c')).read_text()


def function(text, signature):
    """Extract complete C functions while ignoring literal/comment braces."""
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


signatures = ['static Long\nfoldint32(', 'static int\nfoldrealint(',
              'static int\nfoldrealtofixed(', 'static int\nfoldfixedcast(',
              'double\nfix2real(', 'Node*\nfoldcast(', 'Node*\nfoldcasti(']
if 'static int\nfoldstringint(' in source:
    signatures.insert(0, 'static int\nfoldstringint(')
parts = '\n'.join(function(source, signature) for signature in signatures)
parts = re.sub(r'\bstrtoll\(', 'native_strtoll(', parts)
base = (root / 'limbo/tests/fold-casts.c').read_text().split('/* Use actual VM conversion code')[0]
base = base.replace(function(base, 'static Long\nstrtoi('), 'STRING_PARSERS')
lex = (root / 'limbo/lex.c').read_text()
constants = lex[lex.index('enum\n'):lex.index('typedef\tstruct Keywd')]
parser = re.sub(r'^#include[^\n]*\n', '', (root / 'lib9/strtoll.c').read_text(), flags=re.M)
parser = re.sub(r'\bstrtoll\(', 'native_strtoll(', parser)
parsers = '''typedef unsigned char uchar;
'''+constants+'''
static char map[256];
static int lexerrors;
/* Lexer diagnostics must never be used for a cast from arbitrary string data. */
static void yyerror(char *format, ...) { (void)format; lexerrors++; }
'''+function(lex, 'int\ncmap(')+'\n'+function(lex, 'Long\nstrtoi(')+'\n'+parser
base = base.replace('STRING_PARSERS', parsers).replace('IMPLEMENTATION', parts)
types = (root / 'limbo/types.c').read_text()
a = types.index('#define I(x)'); b = types.index('\nint\nispoly(', a)
base = base.replace('SELECTBODY', types[a:b])
vm = function((root / 'libinterp/string.c').read_text(), 'OP(cvtcw)')
vm = re.sub(r'\bstrtol\(', 'modelstrtol(', vm)
vm += '\n'+function((root / 'libinterp/xec.c').read_text(), 'OP(cvtcl)')
vm = re.sub(r'\bstrtoll\(', 'native_strtoll(', vm)
texts = [b'', b' ', b'\t\n\r\f\v -12suffix', b'+', b'-', b'+x', b'--1', b'words',
         b'12suffix', b'-12.5', b'00123', b'0x10', b'123\0tail', b'\0-99', b'\xff12']
for value in (-2**64-1, -2**64, -2**63-1, -2**63, -2**31-1, -2**31, -1, 0,
              255, 256, 2**31-1, 2**31, 2**32-1, 2**63-1, 2**63, 2**64-1, 2**64, 2**64+1):
    for prefix in (b'', b' ', b'\t+'):
        texts.append(prefix+str(value).encode()+b'end')
rng = random.Random(0x737472696e67)
for _ in range(2048):
    digits = bytes(rng.randrange(48, 58) for _ in range(rng.randrange(0, 200)))
    texts.append(rng.choice([b'', b' ', b'\t\r', b'\v\n'])+rng.choice([b'', b'+', b'-'])+digits+
                 rng.choice([b'', b'tail', b'.5', b'\0tail', b' ', b'\xff']))
if os.environ.get('FOLD_STRING_CASE') == 'range':
    texts = [b'2147483648', b'-2147483649', b'999999999999999999999999999999999999']
elif os.environ.get('FOLD_STRING_CASE') == 'prefix':
    texts = [b' \t-12tail', b'+', b'words']
vectors = []
for text in texts:
    match = re.match(rb'[ \t\n\r\f\v]*([+-]?)([0-9]+)', text)
    value = int(match[1]+match[2]) if match else 0
    bits = value % 2**64
    small = -2**31 <= value < 2**31
    word32 = max(-2**31, min(2**31-1, value))
    word64 = max(-2**63, min(2**63-1, value))
    data = '"'+''.join('\\%03o'%c for c in text)+'"'
    vectors.append('{'+','.join([data, str(len(text)), 'UINT64_C('+str(bits)+')',
                                 str(int(small)), str(value if small else 0),
                                 str(word32), '(-INT64_C(9223372036854775807)-1)' if word64 == -2**63 else
                                 'INT64_C('+str(word64)+')'])+'}')
unit = base+(root / 'limbo/tests/fold-strings.c').read_text()
unit = unit.replace('STRING_VM', vm).replace('STRING_VECTORS', ',\n'.join(vectors))
with tempfile.TemporaryDirectory(prefix='fold-strings-') as directory:
    td = Path(directory); target = td / 'strings.c'; target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (32, 64):
        binary = td / ('strings'+str(width))
        subprocess.run(cmd+['-DWORD_BITS='+str(width), '-I'+str(root / 'include'), str(target), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=30)
