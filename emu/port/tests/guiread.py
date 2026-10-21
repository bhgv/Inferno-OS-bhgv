#!/usr/bin/env python3
"""Compile production GUI reads/queue/close with real headers and lib9 fmt."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'emu/port/devgui.c').read_text()

def function(name,result):
    start=source.index(result+'\n'+name+'(')
    brace=source.index('{',start)
    depth=0
    # Ignore braces in strings and comments.
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        value=token.group()
        if value=='{':depth+=1
        elif value=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise ValueError(name)

harness=(root/'emu/port/tests/guiread.c').read_text()
parts={
 'GUIQID_IMPLEMENTATION':source[source.index('#define\tQSHIFT'):source.index('/* Protected by acquire')],
 'GUIRESULT_IMPLEMENTATION':source[source.index('typedef struct _rslt_string'):source.index('extern char rootdir[];')],
 'QID_HELPERS_IMPLEMENTATION':source[source.index('static Tk*'):source.index('// VV dinamic names-tree helper')],
 'GUIGEN_IMPLEMENTATION':source[source.index('/* go to lower dir'):source.index('// AA dinamic names-tree helper')],
 'GUICHANGE_IMPLEMENTATION':source[source.index('typedef struct GuiChange'):source.index('typedef struct GuiSnapshot')],
 'GUILIFECYCLE_TESTS':(root/'emu/port/tests/guilifecycle.c').read_text(),
 'GUISNAPSHOT_IMPLEMENTATION':source[source.index('typedef struct GuiSnapshot'):source.index('static long\nguiread(')],
 'GUIREAD_IMPLEMENTATION':function('guiread','static long'),
 'GUICLOSE_IMPLEMENTATION':function('guiclose','static void'),
 'GUIWRITE_IMPLEMENTATION':source[source.index('typedef struct GuiWrite'):source.index('static int\nguiwstat(')],
 'GUIWRITE_TESTS':(root/'emu/port/tests/guiwrite.c').read_text(),
}
for placeholder,value in parts.items():harness=harness.replace(placeholder,value)
formatters=['dofmt','fmt','fmtquote','fmtrune','rune','runestrlen','utflen','utfnlen',
            'snprint','vsnprint','fmtstr','vsmprint','smprint','fmtprint','fmtvprint']
with tempfile.TemporaryDirectory(prefix='guiread-') as td:
    td=Path(td);unit=td/'guiread.c';unit.write_text(harness)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g'))
    command += ['-Dmalloc=guimalloc','-Drealloc=guirealloc','-Dfree=guifree']
    command += ['-I'+str(root/p) for p in ['Linux/386/include','emu/port','include','libtk','lib9','libinterp']]
    command += [str(unit)]+[str(root/'lib9'/(name+'.c')) for name in formatters]+['-o',str(td/'guiread')]
    subprocess.run(command,check=True)
    subprocess.run([str(td/'guiread')],check=True)
