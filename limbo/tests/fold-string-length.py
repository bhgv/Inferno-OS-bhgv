#!/usr/bin/env python3
"""Check constant len against real UTF decoding, string loading and VM length."""
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
    """Extract balanced C functions, ignoring comments and literals."""
    start = text.index(signature); brace = text.index('{', start); depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


names = ['static vlong\nipow(', 'double\nrpow(', 'int\nsametree(', 'int\nhasside(',
         'Node*\nfolds(', 'Node*\nefold(', 'Node*\nfoldvc(', 'Node*\nfoldc(', 'Node*\nfoldr(']
if 'static Long\nfoldstringlen(' in source:
    names.insert(0, 'static Long\nfoldstringlen(')
parts = '\n'.join(function(source, name) for name in names)
base = (root / 'limbo/tests/fold-identities.c').read_text()
base = base.replace(function(base, 'static int\nutflen('), 'extern int utflen(char*);')
base = base.replace(function(base, 'static Decl*\nstringcat('), 'CONCATENATION')
base = base.replace(function(base, 'static Node*\nmksconst('), 'STRING_CONSTANT')
base = base.replace('main(void)', 'identity_main(void)')
ops = sorted(set(re.findall(r'\bO\w+\b', parts)) | {'Ocall'})
base = base.replace('OPS', 'enum {'+','.join(ops)+',Nops};').replace('IMPLEMENTATION', parts)
lex = (root / 'limbo/lex.c').read_text()
concat = '''static char concatbytes[8192];
static int concatlength;
static Sym concatsymbols[16];
static int concatsymbolcount;
/* Supply static compiler allocation storage without changing production concatenation. */
static void*
allocmem(int n)
{
    char *p;

    assert(n > 0 && concatlength+n <= sizeof concatbytes);
    p = concatbytes+concatlength;
    concatlength += n;
    return p;
}
/* Interning identity is irrelevant here; preserve the exact symbol bytes and length. */
static Sym*
enterstring(char *text, int n)
{
    Sym *s;

    assert(concatsymbolcount < 16);
    s = &concatsymbols[concatsymbolcount++];
    s->name = text;
    s->len = n;
    return s;
}
'''+function(lex, 'Sym*\nstringcat(')
constant = '''/* Construct bounded string constants around production concatenated symbols. */
static Node*
mksconst(Src *src, Sym *sym)
{
    static Decl declarations[16];
    Node *n;

    assert(used < 256 && concatsymbolcount <= 16);
    n = &arena[used++];
    memset(n, 0, sizeof(*n));
    n->op = Oconst;
    n->ty = tstring;
    n->src = *src;
    n->decl = &declarations[concatsymbolcount-1];
    n->decl->sym = sym;
    return n;
}
'''
base = base.replace('CONCATENATION', concat).replace('STRING_CONSTANT', constant)
strings = (root / 'libinterp/string.c').read_text()
vm = function(strings, 'String*\nc2string(')+'\n'+function(strings, 'OP(lenc)')
vm = re.sub(r'\berror\(', 'vmerror(', vm)
texts = ['', '\0', 'a\0b', '\0\0', 'abc\0', '\0abc', 'Ж\0€😀', 'a\0\0Ж\0😀\0']
rng = random.Random(0x6c656e737472)
choices = ['\0', 'a', 'b', 'é', 'Ж', '€', '😀', '\u0301']
for _ in range(1024):
    texts.append(''.join(rng.choice(choices) for _ in range(rng.randrange(129))))
vectors = []
for text in texts:
    split = rng.randrange(len(text)+1)
    left = text[:split].encode(); right = text[split:].encode()
    literal = lambda data: '"'+''.join('\\%03o'%c for c in data)+'"'
    vectors.append('{'+','.join([literal(left), str(len(left)), literal(right), str(len(right)), str(len(text))])+'}')
unit = base+(root / 'limbo/tests/fold-string-length.c').read_text()
unit = unit.replace('STRING_LENGTH_VM', vm).replace('LENGTH_VECTORS', ',\n'.join(vectors))
with tempfile.TemporaryDirectory(prefix='fold-string-length-') as directory:
    td = Path(directory); target = td / 'length.c'; target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (32, 64):
        binary = td / ('length'+str(width))
        subprocess.run(cmd+['-DWORD_BITS='+str(width), '-I'+str(root/'Linux/arm/include'), '-I'+str(root/'include'),
                           str(target), str(root/'lib9/rune.c'), str(root/'lib9/utflen.c'),
                           '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=30)
