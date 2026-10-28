#!/usr/bin/env python3
"""Execute descriptor-producing Sys builtins with failure/ownership adapters."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]

PORTS = os.environ.get('FD_OWNER_PORTS', 'emu os').split()
MODELS = os.environ.get('FD_OWNER_MODELS', '64 32').split()
if not PORTS or any(port not in ('emu', 'os') for port in PORTS):
    raise ValueError('unknown native Sys port')
if not MODELS or any(model not in ('64', '32') for model in MODELS):
    raise ValueError('unknown native Sys WORD model')



def function(text, signature):
    """Extract complete functions without counting braces in comments or literals."""
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


strings = (root / 'libinterp/string.c').read_text()
runt = (root / 'libinterp/runt.c').read_text()
parts = [function(strings, 'String*\n'+name+'(') for name in ('newstring','newrunes','c2string')]
parts += [function(strings, 'char*\nstring2c(').replace('string2c(String *s)', 'capturestring2c(String *s)'), function(runt, 'void\nretstr(')]
parts.insert(0, 'static void capturecallback(int);')
parts.insert(5, '/* Observe callbacks before the real native string-view adapter. */\nchar*\nstring2c(String *s)\n{\n\tcapturecallback(6);\n\treturn capturestring2c(s);\n}')
with tempfile.TemporaryDirectory(prefix='sys-fd-owners-') as directory:
    td = Path(directory)
    for platform in PORTS:
        source = Path(os.environ.get('FD_OWNER_SOURCE', root / platform / 'port/inferno.c')).read_text()
        guard = (root / platform / 'port/dis.c').read_text()
        bodies = [function(guard, 'int\nkprotect(')]
        bodies += [function(source, 'void\nfreeFD('), function(source, 'Sys_FD*\nmkfd(')]
        if 'typedef struct FDResult FDResult;' in source:
            start = source.index('typedef struct FDResult FDResult;')
            end = source.index('\n/* Mark a descriptor', start)
            bodies += [source[start:end]]
            if 'static uchar*\nconnectionresultslot(' in source:
                bodies.append(function(source, 'static uchar*\nconnectionresultslot('))
            bodies += [function(source, sig) for sig in
                       ('static Sys_FD*\ntakefd(', 'static void\nfreefdresult(',
                        'static void\nclearconnection(', 'static void\nmakeconnection(')]
        bodies += [function(source, sig) for sig in ('static void\nsysstatusframe(','static void*\nsysresultowner(', 'static void\nsysfdresultowners(') if sig in source]
        bodies += [function(source, 'static int\nsysnativeint(')]
        bodies += [function(source, 'void\nSys_'+name+'(') for name in
                   ('open','create','fildes','pipe','dial','announce','listen')]
        target = td / 'owners.c'
        template = (root / 'libinterp/tests/sys-fd-owners.c').read_text()
        start = source.index('typedef struct FD FD;')
        end = source.index('\n};', start)+3
        template = template.replace('FD_DECLARATION', source[start:end])
        target.write_text(template.replace('IMPLEMENTATION','\n'.join(parts+bodies)))
        cmd = shlex.split(os.environ.get('CC','cc'))
        cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        for bits in map(int, MODELS):
            includes = []
            if bits == 32:
                model = td / 'word32'
                model.mkdir(exist_ok=True)
                header = (root / 'include/interp.h').read_text()
                header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                (model / 'interp.h').write_text(header)
                includes = ['-I'+str(model)]
            includes += ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary = td / 'owners'
            subprocess.run(cmd+includes+['-pthread',str(target),str(root / 'lib9/rune.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)]+os.environ.get('FD_OWNER_CASE','').split(),check=True)
            print(platform, 'WORD'+str(bits), flush=True)
