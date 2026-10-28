#!/usr/bin/env python3
"""Check priority emitter layout arithmetic and atomic bounded code publication."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
AMD64_HOST = platform.machine().lower() in ('amd64', 'x86_64')
if os.environ.get('EMIT_LAYOUT_ARCH', 'amd64') not in ('amd64', 'arm64'):
    raise ValueError('EMIT_LAYOUT_ARCH must be amd64 or arm64')
if os.environ.get('EMIT_LAYOUT_WORD', '64') not in ('32', '64'):
    raise ValueError('EMIT_LAYOUT_WORD must be 32 or 64')
if os.environ.get('EMIT_LAYOUT_CASE', 'array') not in ('array', 'publication', 'span', 'callbacks', 'owners'):
    raise ValueError('EMIT_LAYOUT_CASE must be array, publication, span, callbacks or owners')
if os.environ.get('EMIT_LAYOUT_CASE') == 'span' and (not AMD64_HOST or os.environ.get('EMIT_LAYOUT_ARCH') == 'arm64'):
    raise ValueError('span execution requires the AMD64 host backend')
shared = (ROOT / 'libinterp/leaf64-ops.h').read_text()
structure = shared[shared.index('struct LeafLayout {'):shared.index('typedef struct LeafOp')]
structure = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*', '', structure)
fields = []
for declaration in re.findall(r'\bint\s+([^;]+);', structure):
    fields.extend(name.strip() for name in declaration.split(','))
compiler = (ROOT / 'libinterp/comp-leaf64.h').read_text()
initialization = compiler[compiler.index('\tabi.wordbytes='):compiler.index('\t/* Cache span sizing')]
identities = {}


# Stable non-null identities are encoded only; this fixture never calls emitted code.
def identity(match):
    name = match.group(0)
    identities.setdefault(name, 0x10000+16*len(identities))
    return '(void*)'+str(identities[name])


initialization = re.sub(r'\(void\*\)\w+|&(?:T\w+|mutator|nprop)\b', identity, initialization)
fixture = (ROOT / 'libinterp/tests/emitter-layouts.c').read_text()
fixture = fixture.replace('LAYOUT_INITIALIZATION', initialization)
fixture = fixture.replace('LAYOUT_FIELDS', ',\n'.join('    offsetof(LeafLayout, '+name+')' for name in fields))
with tempfile.TemporaryDirectory(prefix='emitter-layouts-') as temporary:
    work = Path(temporary)
    for arch in ('amd64', 'arm64'):
        if arch == 'arm64' and os.environ.get('EMIT_LAYOUT_CASE') == 'span':
            continue
        if os.environ.get('EMIT_LAYOUT_ARCH', arch) != arch:
            continue
        for bits in (32, 64):
            if int(os.environ.get('EMIT_LAYOUT_WORD', bits)) != bits:
                continue
            model = work / (arch+str(bits))
            model.mkdir()
            header = (ROOT / 'include/interp.h').read_text()
            if bits == 32:
                header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (model / 'interp.h').write_text(header)
            source = model / 'layouts.c'
            prefix = '#define TEST_AMD64 1 // Enable real host span execution.\n' if arch == 'amd64' and AMD64_HOST else ''
            if arch == 'arm64':
                prefix += '#define EMITTER_ARM64 1 // ARM64 instruction-target alignment.\n'
            source.write_text(prefix+fixture.replace('EMITTER_HEADER', arch+'-emit.h'))
            binary = model / 'layouts'
            command = shlex.split(os.environ.get('CC', 'cc'))
            command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
            includes = ['-I'+str(model), '-I'+os.environ.get('EMIT_LAYOUT_HEADERS', str(ROOT / 'libinterp'))]
            includes += ['-I'+str(ROOT / path) for path in ('Linux/arm/include', 'include', 'libinterp')]
            subprocess.run(command+includes+[str(source), '-o', str(binary)], check=True)
            print(arch+' WORD'+str(bits), flush=True)
            subprocess.run([str(binary)], check=True)
