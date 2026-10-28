#!/usr/bin/env python3
"""Exercise production VM formatting with real lib9 formatting and error frames."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('SYS_PRINT_SOURCE', root / 'libinterp/runt.c')).read_text()


# Ignore braces in comments and literals when extracting production functions.
def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


start = source.index('typedef struct XprintCall')
end = source.index('/* Keep the allocated output', start)
parts = [source[start:end]]
if 'typedef struct XprintString' in source:
    a=source.index('typedef struct XprintString')
    b=source.index('static void\nxprintstring(',a)
    parts.insert(0,source[a:b])
if 'typedef struct XprintResult' in source:
    a = source.index('typedef struct XprintResult')
    b = source.index('/* Build a new VM owner', a)
    parts.append(source[a:b])
for signature in ('static void\nxprintstring(', 'static uchar*\nxprintargument(', 'static int\nxsnprint(', 'int\nxprint(', 'static int\nxprintcall(', 'int\nbigxprint(',
                  'static int\nbuildprintresult(', 'static void*\nprintresult(',
                  'void\nSys_sprint(', 'void\nSys_aprint('):
    if signature in source:
        parts.append(function(source, signature))
io_source = os.environ.get('SYS_PRINT_IO_SOURCE')
if io_source:
    port = Path(io_source).read_text()
    parts += ['char Ebadarg[]="bad argument"; // Controlled native argument-error identity.']
    parts += [next(line for line in port.splitlines() if line.startswith('#define fdchk('))]
    parts += [function(port, signature) for signature in ('static int\nsysnativeint(',
              'void\nSys_print(', 'void\nSys_fprint(', 'void\nSys_werrstr(')]
guard = Path(os.environ.get('SYS_PRINT_GUARD_SOURCE', root/'emu/port/dis.c')).read_text()
guard = '\n'.join(function(guard, signature) for signature in ('int\nkprotect(', 'void\nkrethrow('))
unit = (root / 'libinterp/tests/sys-print.c').read_text()
extra=os.environ.get('SYS_PRINT_EXTRA')
if extra:
    unit=unit.replace('static void*\nprintworker(', 'static int printspanmain(void);\n\nstatic void*\nprintworker(',1)
    unit=unit.replace('*status=printmain();','*status=printspanmain();',1)+Path(extra).read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts)).replace('GUARD', guard)
with tempfile.TemporaryDirectory(prefix='sys-print-') as directory:
    td = Path(directory)
    target = td / 'codecs.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    for model in os.environ.get('SYS_PRINT_MODELS', 'native word32').split():
        override = []
        if model == 'word32':
            modeldir = td / model
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;',
                               'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or m != 1:raise RuntimeError('WORD32 header mismatch')
            (modeldir / 'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td / ('codecs-'+model)
        subprocess.run(cmd+override+includes+[str(target),*[str(root / ('lib9/'+p+'.c')) for p in ('rune','sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')],'-pthread','-lm','-o',str(binary)],check=True)
        families=[os.environ['SYS_PRINT_CASE']] if os.environ.get('SYS_PRINT_CASE') else ['buffer','arguments','metadata','packed','heap','growth'] if extra else ['alignment','invalid','embedded','legacy']
        for family in families:
            subprocess.run([str(binary)],env=dict(os.environ,SYS_PRINT_CASE=family),check=True)
