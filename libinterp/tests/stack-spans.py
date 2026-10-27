#!/usr/bin/env python3
"""Test real stack operations with native pointer-span, frame-map and allocation-capture oracles."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]  # Owning interpreter source tree.
fixture=(ROOT/'libinterp/tests/stack-lifecycle.c').read_text()
fixture=fixture.replace('#include <setjmp.h>', '#include <setjmp.h>\n\n\n/* Keep complete span oracles active at native error boundaries. */\nstatic void\nspancondition(int condition)\n{\n    if(!condition)abort();\n}\n#define spancheck(condition) spancondition(!!(condition)) // Normalize pointer and scalar oracles.\n',1)
fixture=fixture.replace('main(int argc,char **argv)', 'spanlegacy(int argc,char **argv)',1)
fixture=fixture.replace('static ulong ownedsize;', '''static ulong ownedsize;
static Type *spantype; // Optional descriptor whose scalar size changes at allocation.
static int spannewsize; // Replacement size injected after allocation was already bounded.
static int spanregisters; // Optional allocator observer changing live REG cursors.
''')
fixture=fixture.replace('owned=p;ownedsize=n;', '''owned=p;ownedsize=n;
    if(spantype != nil)spantype->size=spannewsize;
    if(spanregisters){R.SP=(uchar*)11;R.TS=(uchar*)12;R.EX=(uchar*)13;}
''',1)
fixture+=(ROOT/'libinterp/tests/stack-spans.c').read_text()
with tempfile.TemporaryDirectory(prefix='stack-spans-') as directory:
    target=Path(directory)/'spans.c';target.write_text(fixture)
    env=dict(os.environ,LIFECYCLE_FIXTURE=str(target))
    env.setdefault('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
    for family in os.environ.get('STACK_SPAN_CASES','maps geometry capture').split():
        subprocess.run([sys.executable,str(ROOT/'libinterp/tests/stack-lifecycle.py')],env=dict(env,LIFECYCLE_CASE=family),check=True)
