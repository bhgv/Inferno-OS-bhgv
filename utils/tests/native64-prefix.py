#!/usr/bin/env python3
"""Generate non-callable variadic prefix schemas and verify their C/GC geometry."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fstrict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='native64-prefix-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory); env = dict(os.environ, TMPDIR=str(work))
        probe = work/'prefix.m'
        probe.write_text('Prefix: module { Pair: adt { n: int; s: string; }; '
                         'Node: adt { tag: int; format: fn(this: self ref Node, fmt: string, *): string; }; '
                         'mixed: fn(b: byte, n: int, wide: big, r: real, fmt: string, *): string; '
                         'record: fn(value: Pair, fmt: string, *): string; '
                         'many: fn('+','.join('a'+str(i)+': string' for i in range(150))+', *): string; };')
        only = work/'only.m'; only.write_text('Only: module { f: fn(fmt: string, *): int; };')
        for label, source in (('sys', ROOT/'module/sys.m'), ('prefix', probe), ('only', only)):
            command = [str(args.limbo.resolve()), '-H', str(source)]
            header = subprocess.check_output(command, env=env)
            if header != subprocess.check_output(command, env=env): raise RuntimeError('Nondeterministic prefix schemas')
            (work/(label+'-native64.h')).write_bytes(header)
        sys = (work/'sys-native64.h').read_text()
        if len(re.findall('static const LimboNative64Prefix limbo_native64_Sys_', sys)) != 4:
            raise RuntimeError('Expected four Sys variadic prefixes')
        for name in ('print', 'fprint', 'sprint', 'aprint'):
            if 'limbo_native64_Sys_'+name+'_entry' in sys:
                raise RuntimeError('Variadic prefix leaked into fixed callable entries')
        (work/'test.c').write_text((ROOT/'utils/tests/native64-prefix.c').read_text())
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include')]
        for label, model in (('WORD32', ['-DDIS_WORD32=1']), ('file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])):
            binary = work/label
            subprocess.run(compiler+flags+model+includes+[str(work/'test.c'), '-o', str(binary)], env=env, check=True)
            subprocess.run(runner+[str(binary)], env=env, check=True, timeout=60)
        # Prefixes are opt-in and cannot silently introduce new declarations in ordinary consumers.
        source = work/'no-prefix.c'
        source.write_text('#include "lib9.h"\n#include "isa.h"\n#include "interp.h"\n#include "only-native64.h"\n'
                          'void *bad = (void*)&limbo_native64_Only_f_prefix;\n')
        result = subprocess.run(compiler+flags+['-DDIS_WORD32=1']+includes+['-fsyntax-only', str(source)], env=env, capture_output=True, text=True)
        if result.returncode == 0 or 'limbo_native64_Only_f_prefix' not in result.stderr:
            raise RuntimeError('Prefix opt-in was bypassed')
        tutorial = (ROOT/'doc/dis64-native64-prefixes.md').read_text()
        examples = re.findall(r'```c\n(.*?)```', tutorial, re.S)
        if len(examples) != 5: raise RuntimeError('Expected all five complete C tutorial examples')
        source = work/'tutorial.c'; source.write_text('\n'.join(examples))
        subprocess.run(compiler+flags+['-DDIS_WORD32=1', '-Werror=implicit-function-declaration']+includes+
                       ['-c', str(source), '-o', str(work/'tutorial.o')], env=env, check=True)
        print('PASS generated prefix determinism, four Sys schemas, no callable entries and explicit opt-in')


if __name__ == '__main__':
    main()
