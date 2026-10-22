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
          for name in ['IPint_and', 'IPint_ori', 'IPint_xor', 'IPint_not', 'IPint_random',
                       'IPints_probably_prime', 'IPints_genprime']]
if 'typedef struct IPintPrimePair' in ipint:
    parts += [ipint[ipint.index('typedef struct IPintPrimePair'):ipint.index('\nvoid\nIPints_DSAprimes(')]]
else:
    parts += [function('libinterp/ipint.c', 'void\nIPints_gensafeprime(')]
parts += [function('libinterp/ipint.c', 'static IPints_IPint*\nstrtoipint(')]
if 'static void\nipintfrombytes(' in ipint:
    parts += [function('libinterp/ipint.c', 'static void\nipintfrombytes(')]
parts += [function('libinterp/ipint.c', 'void\n' + name + '(')
          for name in ['IPint_strtoip', 'IPint_b64toip', 'IPint_bytestoip',
                       'IPint_bebytestoip', 'IPint_inttoip', 'IPint_iptoint', 'IPint_bits']]
parts += [function('libinterp/runt.c', 'void\nretstr('),
          function('libinterp/runt.c', 'Array*\nmem2array(')]
unit = (root / 'libinterp/tests/ipint-exports.c').read_text()
unit = unit.replace('TYPES', types).replace('IMPLEMENTATION', '\n'.join(parts))
files = ['mpaux', 'mpfmt', 'strtomp', 'mptov', 'mptobe', 'betomp', 'mpadd', 'mpsub', 'mpcmp',
         'mpmul', 'mprand', 'mpleft', 'mpright', 'mpvecadd', 'mpvecsub', 'mpvecdigmuladd',
         'mpveccmp', 'mpdigdiv', 'mpdiv', 'mpmod', 'mpexp', 'mpextendedgcd', 'mpinvert', 'mptoui', 'mptoi']

with tempfile.TemporaryDirectory(prefix='ipint-exports-') as directory:
    td = Path(directory)
    units = []
    support = '#include "' + str(root / 'libmp/tests/text-support.h') + '"\n'
    support += 'void genrandom(uchar*, int); void prng(uchar*, int);\n'
    support += 'void genprime(mpint*, int, int);\n'
    support += 'int probably_prime(mpint*, int); int smallprimetest(mpint*);\n'
    support += '#define nelem(a) (sizeof(a)/sizeof((a)[0]))\n'
    for path in [root / ('libmp/port/' + name + '.c') for name in files] + [
            root / 'libsec/port/probably_prime.c', root / 'libsec/port/smallprimetest.c',
            root / 'libsec/port/genprime.c', root / 'libsec/port/gensafeprime.c',
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
    includes = ['-I' + str(root / path) for path in
                ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
    for model in ('native', 'word32'):
        override = []
        if model == 'word32':
            # Reuse real VM declarations, changing only the two scalar word typedefs.
            modeldir = td / model
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            header, signed = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, unsigned = re.subn(r'typedef unsigned long\s+UWORD;',
                                      'typedef unsigned int UWORD;', header, count=1)
            assert signed == unsigned == 1
            (modeldir / 'interp.h').write_text(header)
            override = ['-I' + str(modeldir)]
        binary = td / ('exports-' + model)
        subprocess.run(cmd + override + includes + units + [str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
