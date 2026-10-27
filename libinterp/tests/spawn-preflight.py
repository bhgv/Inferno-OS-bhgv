#!/usr/bin/env python3
"""Check actual SPAWN/MSPAWN C handlers and complete native tails with real error/scheduler boundaries."""
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]  # Owning interpreter tree.
machine=platform.machine().lower()
backend='amd64' if machine in ('x86_64','amd64') else 'arm64' if machine in ('aarch64','arm64') else None
if backend is None:
    print('SKIP native spawn tails: requires AMD64 or ARM64')
    raise SystemExit(0)
fixture=(ROOT/'libinterp/tests/stack-startup.c').read_text()
fixture=fixture.replace('#include <assert.h>', '''/* Keep scheduler/error-frame checks active with NDEBUG. */
static void
spawncondition(int condition)
{
    if(!condition)abort();
}
#define spawncheck(condition) spawncondition(!!(condition)) // Normalize inherited pointer and scalar checks.
''').replace('assert(', 'spawncheck(')
fixture=fixture.replace('Import empty;', 'static Import empty; /* Stable fixture sentinel across native error unwinds. */')
fixture=fixture.replace('main(void)','spawnlegacy(void)',1)
source=Path(os.environ.get('STACK_XEC_SOURCE',ROOT/'libinterp/xec.c')).read_text()
start=source.index('int\ncjitexec(');brace=source.index('{',start);depth=0;executor=None
for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
    if token.group()=='{':depth+=1
    elif token.group()=='}':
        depth-=1
        if depth==0:
            executor=source[start:brace+token.end()]
            break
if executor is None:raise RuntimeError('cjitexec')
fixture+='\n#include "'+backend+'-emit.h"\n#include "leaf-code.h"\n'
fixture+='\nvoid (*optab[256])(void); // Production complete-entry handler slots.\nchar exOp[]="opcode"; // Invalid opcode error identity.\n'+executor+'\n'
fixture+=(ROOT/'libinterp/tests/spawn-preflight.c').read_text()
with tempfile.TemporaryDirectory(prefix='spawn-preflight-') as directory:
    target=Path(directory)/'spawn.c';target.write_text(fixture)
    env=dict(os.environ,STACK_FIXTURE=str(target),STACK_CASES='spawnpreflight')
    env.setdefault('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
    subprocess.run([sys.executable,str(ROOT/'libinterp/tests/stack-startup.py')],env=env,check=True)
