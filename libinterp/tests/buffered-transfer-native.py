#!/usr/bin/env python3
"""Execute buffered SEND/RECV and pure geometry with production C and AMD64 RX."""
import os
import platform
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning repository.
SOURCE = (ROOT/'libinterp/xec.c').read_text()  # Production VM bodies.
if platform.machine().lower() not in ('amd64','x86_64'):
    print('SKIP buffered AMD64 transfers: requires an AMD64 host')
    sys.exit(0)


# Extract a complete production C definition without counting comment/literal braces.
def function(signature, source=SOURCE):
    start = source.index(signature)
    brace = source.index('{',start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{': depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0: return source[start:brace+token.end()]
    raise RuntimeError(signature)


parts = [function(s) for s in ('static Type*\nmoduletype(','static int\nmappedcopytype(','static List*\nlisthead(','OP(movmp)','OP(headmp)','void\nmovtmp(')]
parts += [function('OP('+name+')') for name in ('movb','movw','movl','movf','movm')]
parts += [SOURCE[SOURCE.index('typedef struct ChannelBuffer'):SOURCE.index('/* Validate and retain buffered storage')]]
baseline = Path(os.environ['BUFFER_C_SOURCE']).read_text() if os.environ.get('BUFFER_C_SOURCE') else SOURCE
parts += [function('static int\n'+name+'(',baseline) for name in ('cgetb','cputb')]
parts += [function(s) for s in ('static int\nchanneloutside(','int\ncchanleaf(')]
parts += [function('OP('+name+')',baseline) for name in ('isend','irecv')]
unit = '#define TEST_LEAF_TRANSFER_RUNTIME // Link real pure channel preflight.\n'+(ROOT/'libinterp/tests/mapped-copy-native.c').read_text()
unit = unit.replace('main(void)','mappedlegacy(void)',1)
unit = unit.replace('IMPLEMENTATION',(ROOT/'libinterp/tests/buffered-transfer-stubs.h').read_text()+'\n\n'+'\n\n'.join(parts))
unit += (ROOT/'libinterp/tests/buffered-transfer-native.c').read_text()
with tempfile.TemporaryDirectory(prefix='buffered-native-') as temporary:
    td = Path(temporary)
    source = td/'fixture.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (32,64):
        model = td/str(wb)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb==32:
            header = re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('word'+str(wb))
        subprocess.run(command+includes+[str(source),'-lm','-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
