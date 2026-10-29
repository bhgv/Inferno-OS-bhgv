#!/usr/bin/env python3
"""Run real stack, frame, spawn and root-startup code with guarded boundaries."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Ignore literals and comments when extracting the production function bodies.
def function(text, signature):
    start = text.index(signature); brace = text.index('{', start); depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


stack = Path(os.environ.get('STACK_SOURCE', root/'libinterp/stack.c')).read_text()
xec = Path(os.environ.get('STACK_XEC_SOURCE', root/'libinterp/xec.c')).read_text()
heap = (root/'libinterp/heap.c').read_text()
common = [function(heap, 'void\ninitmem('), function(heap, 'void\nfreeptrs(')]
if 'static ulong\nstackhint(' in stack:
    common += [function(stack, 'static ulong\nstackhint(')]
if 'static int\nstacksize(' in stack:
    common += [function(stack, 'static int\nstacksize(')]
common += [function(stack, signature) for signature in ('static Stkext*\nstackextent(', 'static Type*\nstackframe(', 'void\nnewstack(', 'void\nextend(', 'void\nunextend(')]
for signature in ('static int\nmoduleentry(', 'static int\nframefits('):
    if signature in xec:
        common += [function(xec, signature)]
common += [function(xec, 'static Type*\nmoduletype(')]
common += [function(xec, 'OP('+name+')') for name in ('frame', 'mframe')]
if 'typedef struct SpawnProg' in xec:
    common += [xec[xec.index('typedef struct SpawnProg'):xec.index('OP(spawn)')]]
common += [function(xec, 'OP('+name+')') for name in ('spawn', 'mspawn')]
if os.environ.get('STACK_WORD_BITS') and os.environ['STACK_WORD_BITS'] not in ('32','64'):
    raise ValueError('STACK_WORD_BITS must select 32 or 64')
for variant in os.environ.get('STACK_VARIANTS','emu os').split():
    if variant not in ('emu','os'):raise ValueError(variant)
    source = Path(os.environ.get('STACK_DIS_SOURCE', root/variant/'port/dis.c')).read_text()
    parts = ['#include "vmstartup.h"\n'] + [function(source, signature) for signature in ('int\nkprotect(', 'void\nkrethrow(')]
    if 'static int\nnewrootgrp(' in source:
        parts += [function(source, 'static int\nnewrootgrp(')]
    if 'typedef struct NewProgEnv' in source:
        parts += [source[source.index('typedef struct NewProgEnv'):source.index('Prog*\nnewprog(')]]
    parts += [function(source, 'Prog*\nnewprog('), function(source, 'void\nnewgrp(')]
    if 'typedef struct SchedModule' in source:
        parts += [source[source.index('typedef struct SchedModule'):source.index('Prog*\nschedmod(')]]
    parts += [function(source, 'Prog*\nschedmod(')]
    unit = Path(os.environ.get('STACK_FIXTURE',root/'libinterp/tests/stack-startup.c')).read_text().replace('IMPLEMENTATION', '\n'.join(common+parts))
    with tempfile.TemporaryDirectory(prefix='stack-startup-') as directory:
        td = Path(directory); target = td/'test.c'; target.write_text(unit)
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        includes = ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        for model in ('native', 'word32'):
            if os.environ.get('STACK_WORD_BITS') and model != ('word32' if os.environ['STACK_WORD_BITS']=='32' else 'native'):
                continue
            override = []
            if model == 'word32':
                modeldir = td/model; modeldir.mkdir()
                header = (root/'include/interp.h').read_text()
                header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                if n != 1 or m != 1:
                    raise RuntimeError('WORD32 fixture header mismatch')
                (modeldir/'interp.h').write_text(header)
                override = ['-I'+str(modeldir)]
            binary = td/('test-'+model)
            subprocess.run(cmd+override+includes+[str(target), '-o', str(binary)], check=True)
            print(variant, model, flush=True)
            selected = os.environ.get('STACK_CASES')
            if selected != 'pid':
                # Keep independent longjmp families in fresh processes with active stack checking.
                families = [selected] if selected else ['stack','root','spawn','spawnalignment','indices','framealignment','frameerrors','environment','overflow']
                for family in families:
                    environment = dict(os.environ, STACK_CASES=family)
                    print('family', family, flush=True)
                    # Separate C and tail-JIT unwind topologies without disabling ASan stack checks.
                    if family=='spawnpreflight':
                        routes=[environment['SPAWN_ROUTE']] if environment.get('SPAWN_ROUTE') else ['0','1']
                        imports=[environment['SPAWN_IMPORTED']] if environment.get('SPAWN_IMPORTED') else ['0','1']
                        for route in routes:
                            for imported in imports:
                                selected=dict(environment,SPAWN_ROUTE=route,SPAWN_IMPORTED=imported)
                                subprocess.run([str(binary)],check=True,env=selected,timeout=60)
                                if not selected.get('SPAWN_PREFLIGHT_CASE'):
                                    subprocess.run([str(binary)],check=True,env=dict(selected,SPAWN_PREFLIGHT_CASE='legacy'),timeout=60)
                    else:
                        subprocess.run([str(binary)], check=True, env=environment, timeout=60)

            # Seed only the private counter to test exhaustion without billions of spawns.
            pid_unit = unit.replace('static int pidnum;', 'static int pidnum = 0x7fffffff;')
            if pid_unit == unit:
                raise RuntimeError('pid exhaustion fixture did not seed the private counter')
            target.write_text(pid_unit)
            pid_binary = td/('pid-'+model)
            subprocess.run(cmd+override+includes+[str(target), '-o', str(pid_binary)], check=True)
            environment = dict(os.environ, STACK_CASES='pid')
            subprocess.run([str(pid_binary)], check=True, env=environment, timeout=30)
            target.write_text(unit)
