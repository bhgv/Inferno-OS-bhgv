#!/usr/bin/env python3
"""Run real pctl descriptor groups with ownership and native-width adapters."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract source functions without counting braces in literals and comments."""
    start=source.index(signature)
    brace=source.index('{',start)
    depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise ValueError(signature)


template=(ROOT/'libinterp/tests/sys-pctl.c').read_text()
with tempfile.TemporaryDirectory(prefix='sys-pctl-') as directory:
    td=Path(directory)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for platform in ('emu','os'):
        source=Path(os.environ.get('PCTL_SOURCE',ROOT/platform/'port/inferno.c')).read_text()
        guard=(ROOT/platform/'port/dis.c').read_text()
        bodies=[function(guard,'int\nkprotect(')]
        start=source.index('enum\n{\n\t/* the following pctl')
        end=source.index('\n};',start)+3
        bodies += [source[start:end],function(source,'static int\nsysnativeint(')]
        helper=source if 'static int\nsysfdword(' in source else (ROOT/platform/'port/inferno.c').read_text()
        bodies.append(function(helper,'static int\nsysfdword('))
        bodies.append(function(source,'void\nSys_pctl('))
        target=td/(platform+'.c')
        # Adapt Plan 9 anonymous Lock/Ref promotions to the explicit emu host structs.
        bridge=('#define lock(p) lock(&((Fgrp*)(p))->l)\n#define unlock(p) unlock(&((Fgrp*)(p))->l)\n#define incref(p) incref(&((Chan*)(p))->r)\n') if platform=='os' else ''
        target.write_text(template.replace('IMPLEMENTATION',bridge+'\n'.join(bodies)+'\n#undef lock\n#undef unlock\n#undef incref\n'))
        for bits in (64,32):
            model=td/(platform+str(bits))
            model.mkdir()
            header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=td/(platform+str(bits)+'-pctl')
            subprocess.run(command+includes+[str(target),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
