#!/usr/bin/env python3
"""Check actual reference propagation and local sorting with integer/key oracles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MIN = -(1 << 31)
MAX = (1 << 31)-1
FAMILIES = ['adt', 'module', 'member', 'nested', 'nestediface', 'string', 'dasdec', 'dasrestore', 'import']


# Extract complete production functions without counting comment/literal braces.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


# Construct independent arbitrary-precision expectations for real propagation paths.
def refvectors(path, selected):
    values = [MIN, MIN+1, -100, -1, 0, 1, 2, 31, 255, 65535, MAX-1, MAX]
    rows = []
    for action, family in enumerate(FAMILIES):
        if selected not in (None, 'refs', family):
            continue
        for a in values:
            for b in ([0] if action == 6 else values):
                if selected == 'dasrestore' and (a, b) != (1, MAX):
                    continue
                total = a+b
                valid = MIN <= total <= MAX
                if action == 6:
                    total = a
                    valid = a > MIN
                if action == 7:
                    valid = valid and a > MIN and MIN <= a-1+b <= MAX
                rows.append((action, a, b, int(valid), total if valid else 0))
    path.write_text(''.join(' '.join(map(str, row))+'\n' for row in rows))


# Model the comparison as lexicographic priorities, never subtraction in a C int.
def pairvectors(path, width, selected):
    sizes = [0, 1, 4, 8, 16, MAX, MAX+1, MAX+2, (1 << 63)-1, -(1 << 63)]
    counts = [MIN, -1, 0, 1, MAX]
    if selected == 'refsorder':
        sizes = [16]
    if selected == 'sizeorder':
        sizes = [1, MAX+2]
        counts = [0]
    rows = []
    for es in sizes:
        for fs in sizes:
            for er in counts:
                for fr in counts:
                    for links in range(4):
                        for names in range(4):
                            en, fn = ('same', 'z') if names == 0 else ('a', 'same') if names == 1 else ('same', 'same') if names == 2 else ('z', 'a')
                            def key(size, refs, linked, name):
                                return (linked, size > width, size if size <= width else 0, -refs, size, name)
                            e = key(es, er, bool(links & 1), en)
                            f = key(fs, fr, bool(links & 2), fn)
                            rows.append((es, fs, er, fr, links, names, int(e <= f)))
    path.write_text(''.join(' '.join(map(str, row))+'\n' for row in rows))


decls = Path(os.environ.get('REF_SORT_DECLS', ROOT/'limbo/decls.c')).read_text()
com = Path(os.environ.get('REF_SORT_COM', ROOT/'limbo/com.c')).read_text()
parts = []
if 'declrefsum(int count' in decls:
    parts.append(function(decls, 'int\ndeclrefsum('))
parts += [function(decls, sig) for sig in ('void\ncheckrefs(', 'Decl*\npopscope(', 'static void\nfreeloc(')]
if 'declcompare(Decl *e' in com:
    parts.append(function(com, 'static int\ndeclcompare('))
parts += [function(com, sig) for sig in ('static Decl*\ndeclmerge(', 'static Decl*\nrecdeclsort(', 'Decl*\ndeclsort(')]
selected = os.environ.get('REF_SORT_CASE')
with tempfile.TemporaryDirectory(prefix='reference-sort-') as temporary:
    work = Path(temporary)
    source = work/'checks.c'
    fixture = (ROOT/'limbo/tests/reference-sort.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/reference-sort-reference.c').read_text())
    source.write_text(fixture)
    refs = work/'refs.txt'
    refvectors(refs, selected)
    for width in (4, 8):
        model = work/str(width)
        model.mkdir()
        isa = (ROOT/'include/isa.h').read_text()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model/'checks'
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        families = [selected] if selected else ['refs', 'pairs', 'shallow', 'groups']
        for family in families:
            if family in FAMILIES or family == 'refs':
                subprocess.run([str(binary), 'refs', str(refs)], check=True)
            elif family in ('pairs', 'refsorder', 'sizeorder'):
                pairs = model/'pairs.txt'
                pairvectors(pairs, width, family)
                subprocess.run([str(binary), 'pairs', str(pairs)], check=True)
            else:
                subprocess.run([str(binary), family], check=True)
