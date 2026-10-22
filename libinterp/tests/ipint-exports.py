#!/usr/bin/env python3
"""Execute production IPints exports/ownership with real VM frames and libmp."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Extract complete functions, ignoring comments and string braces.
def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace + token.end()]
    raise ValueError(signature)


ipint = (root / 'libinterp/ipint.c').read_text()
types = ipint[ipint.index('struct IPint\n'):ipint.index('Type\t*TIPint;')]
parts = [function('emu/port/dis.c', 'int\nkprotect('),
         function('emu/port/dis.c', 'void\nkrethrow(')]
if 'typedef struct IPintBuild' in ipint:
    parts += [ipint[ipint.index('typedef struct IPintBuild'):ipint.index('\nvoid\nfreeIPint(')]]
    parts += [ipint[ipint.index('typedef struct IPintText'):ipint.index('\nstatic IPints_IPint*\nstrtoipint')]]
    parts += [ipint[ipint.index('typedef struct IPintDivide'):ipint.index('\nvoid\nIPint_neg(')]]
else:
    # Keep baseline execution available to demonstrate the original ownership bug.
    parts += ['enum {MaxBigBytes = 1024};']
    parts += [function('libinterp/ipint.c', 'void*\nnewIPint('),
              function('libinterp/ipint.c', 'mpint*\ncheckIPint(')]
    names = ['IPint_iptob64', 'IPint_iptob64z', 'IPint_iptostr',
             'IPint_iptobebytes', 'IPint_iptobytes', 'IPint_div', 'IPint_mod']
    parts += [function('libinterp/ipint.c', 'void\n' + name + '(') for name in names]
parts += [function('libinterp/ipint.c', 'void\nIPint_expmod(')]
if 'static void\nipintbinary(' in ipint:
    parts += [function('libinterp/ipint.c', 'static void\nipintbinary(')]
parts += [function('libinterp/ipint.c', 'void\n' + name + '(')
          for name in ['IPint_add', 'IPint_sub', 'IPint_mul', 'IPint_neg',
                       'IPint_copy', 'IPint_invert']]
if 'static void\nipintshift(' in ipint:
    parts += [function('libinterp/ipint.c', 'static void\nipintshift(')]
parts += [function('libinterp/ipint.c', 'void\n' + name + '(')
          for name in ['IPint_shl', 'IPint_shr']]
parts += [function('libinterp/ipint.c', 'static void\n' + name + '(')
          for name in ['mpand', 'mpor', 'mpxor', 'mpnot']]
parts += [function('libinterp/ipint.c', 'void\n' + name + '(')
          for name in ['IPint_and', 'IPint_ori', 'IPint_xor', 'IPint_not', 'IPint_random']]
parts += [function('libinterp/runt.c', 'void\nretstr('),
          function('libinterp/runt.c', 'Array*\nmem2array(')]
unit = (root / 'libinterp/tests/ipint-exports.c').read_text()
unit = unit.replace('TYPES', types).replace('IMPLEMENTATION', '\n'.join(parts))
files = ['mpaux', 'mpfmt', 'mptobe', 'betomp', 'mpadd', 'mpsub', 'mpcmp',
         'mpmul', 'mprand', 'mpleft', 'mpright', 'mpvecadd', 'mpvecsub', 'mpvecdigmuladd',
         'mpveccmp', 'mpdigdiv', 'mpdiv', 'mpmod', 'mpexp', 'mpextendedgcd', 'mpinvert', 'mptoui', 'mptoi']

with tempfile.TemporaryDirectory(prefix='ipint-exports-') as directory:
    td = Path(directory)
    units = []
    support = '#include "' + str(root / 'libmp/tests/text-support.h') + '"\n'
    for path in [root / ('libmp/port/' + name + '.c') for name in files] + [
            root / 'lib9/u32.c', root / 'lib9/u64.c', root / 'libmp/tests/text-support.c']:
        text = re.sub(r'^#include[^\n]*\n', '', path.read_text(), flags=re.M)
        target = td / path.name
        target.write_text(support + text)
        units.append(str(target))
    target = td / 'exports.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    for path in ('Linux/arm/include', 'emu/port', 'include', 'libinterp'):
        cmd += ['-I' + str(root / path)]
    subprocess.run(cmd + units + [str(target), '-o', str(td / 'exports')], check=True)
    subprocess.run([str(td / 'exports')], check=True)
