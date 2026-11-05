#!/usr/bin/env python3
"""Compile the AWM startup slice and report the current Dis64 compiler barriers."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STARTUP = (
    'appl/cmd/emuinit.b',
    'appl/wm/awm.b',
    'appl/wm/atoolbar.b',
    'appl/lib/arg.b',
    'appl/lib/string.b',
    'appl/lib/wmsrv.b',
    'appl/lib/wmclient.b',
    'appl/lib/winplace.b',
    'appl/lib/wmlib.b',
    'appl/lib/titlebar.b',
    'appl/lib/tkclient.b',
    'appl/cmd/sh/sh.b',
)


# Capture the real compiler status and diagnostics for a single source.
def compile_module(limbo, source, target, options):
    return subprocess.run(
        [str(limbo), *options, '-I', 'module', '-o', str(target), source],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, timeout=60,
    )


def check_builtin_imports(limbo, work):
    """Compare actual LOAD import groups with the generated C export tables."""
    config = (ROOT/'emu/Android/emu').read_text()
    mods = re.search(r'^mod\n((?:[ \t]+[^\n]*\n|\n)+)', config, re.M)
    if mods is None:
        raise RuntimeError('Android configuration has no mod section')
    builtins = {line.split()[0] for line in mods[1].splitlines() if line.strip()}

    assembly = work/'awm.s'
    result = compile_module(limbo, 'appl/wm/awm.b', assembly, ['-S'])
    if result.returncode:
        raise RuntimeError(result.stdout)
    code = assembly.read_text()
    paths = dict(re.findall(r'^\s*string\s+@mp\+(\d+),"([^"]+)"$', code, re.M))
    groups = re.split(r'^\s*word\s+@ldt\+(\d+),(\d+)\s*$', code, flags=re.M)
    tables = {}
    for ordinal, count, body in zip(groups[1::3], groups[2::3], groups[3::3]):
        imports = re.findall(r'^\s*ext\s+@ldt\+\d+,(0x[0-9a-f]+),"([^"]+)"$', body, re.M)
        if len(imports) != int(count):
            raise RuntimeError(f'AWM import table {ordinal} has the wrong size')
        tables[ordinal] = imports

    checked = set()
    for offset, ordinal in re.findall(r'^\s*load\s+(\d+)\(mp\),\$(\d+),', code, re.M):
        path = paths.get(offset, '')
        if not path.startswith('$'):
            continue
        module = path[1:]
        if module.lower() not in builtins:
            raise RuntimeError(f'AWM loads {path}, absent from the Android mod section')
        # Use the same generator and declaration root as libinterp/mkfile.
        exports = subprocess.check_output(
            [str(limbo), '-t', module, '-I', 'module', 'module/runt.m'],
            cwd=ROOT, text=True, timeout=60,
        )
        signatures = {name: int(value, 16) for name, value in re.findall(
            r'^\s*"([^"]+)",(0x[0-9a-f]+),', exports, re.M)}
        for value, name in tables[ordinal]:
            if signatures.get(name) != int(value, 16):
                raise RuntimeError(f'AWM import {path}->{name} does not match generated exports')
        checked.add(path)
        print(f'PASS AWM {path} import group {ordinal}: {len(tables[ordinal])} generated signatures')
    if not {'$Sys', '$Draw'} <= checked:
        raise RuntimeError('AWM startup assembly is missing the Sys or Draw LOAD')


def check_string_frames(limbo, work):
    """Verify the two managed String call shapes before assigning wire kinds."""
    header = subprocess.check_output(
        [str(limbo), '-H', '-M', 'String', '-I', 'module', 'module/string.m'],
        cwd=ROOT, timeout=60,
    )
    (work/'string-native64.h').write_bytes(header)
    source = work/'string-frames.c'
    source.write_text(r'''
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "string-native64.h"

#define FIELD_IS(frame, field, type) _Generic(((frame*)0)->field, type: 1, default: 0)
_Static_assert(sizeof(F_String_quoted) == 72 && sizeof(F_String_unquoted) == 72,
    "managed frame extent");
_Static_assert(offsetof(F_String_quoted, argv) == 64 &&
    offsetof(F_String_unquoted, args) == 64, "mapped argument offset");
_Static_assert(offsetof(F_String_quoted, ret) == 32 &&
    offsetof(F_String_unquoted, ret) == 32, "transient result address offset");
_Static_assert(FIELD_IS(F_String_quoted, argv, List*) &&
    FIELD_IS(F_String_quoted, ret, String**), "quoted: list -> string");
_Static_assert(FIELD_IS(F_String_unquoted, args, String*) &&
    FIELD_IS(F_String_unquoted, ret, List**), "unquoted: string -> list");

int main(void)
{
    static const uchar quoted[] = F_String_quoted_map;
    static const uchar unquoted[] = F_String_unquoted_map;
    return !(sizeof(quoted) == 2 && sizeof(unquoted) == 2 &&
        quoted[0] == 0 && quoted[1] == 0x80 &&
        unquoted[0] == 0 && unquoted[1] == 0x80);
}
''')
    binary = work/'string-frames'
    command = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run(command+['-std=gnu11', '-DDIS_WORD32=1', '-I', 'Linux/386/include',
                           '-I', 'include', str(source), '-o', str(binary)],
                   cwd=ROOT, check=True, timeout=60)
    subprocess.run([str(binary)], check=True, timeout=30)
    print('PASS generated String quoted/unquoted: typed arguments, result addresses and map8')


# Keep ordinary source compilation and experimental Dis64 admission separate.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    limbo = args.limbo.resolve()
    refused = 0
    with tempfile.TemporaryDirectory(prefix='awm-preflight-', dir=ROOT) as directory:
        work = Path(directory)
        check_builtin_imports(limbo, work)
        check_string_frames(limbo, work)
        for source in STARTUP:
            legacy = work / (Path(source).stem + '.dis')
            result = compile_module(limbo, source, legacy, [])
            if result.returncode != 0 or not legacy.is_file() or legacy.stat().st_size == 0:
                raise RuntimeError(f'{source}: ordinary compilation failed\n{result.stdout}')
            target = legacy.with_suffix('.dis64')
            result = compile_module(limbo, source, target, ['-64'])
            if result.returncode == 0:
                if not target.is_file() or target.stat().st_size == 0:
                    raise RuntimeError(f'{source}: Dis64 compiler omitted its output')
                print(f'{source}: emitted Dis64')
                continue
            diagnostic = next((line for line in result.stdout.splitlines()
                               if 'Dis64 scalar:' in line), '')
            if result.returncode < 0 or target.exists() or not diagnostic:
                raise RuntimeError(f'{source}: unexpected Dis64 refusal\n{result.stdout}')
            if source == 'appl/wm/awm.b':
                witness = re.search(r':(\d+): Dis64 scalar: module load requires managed linkage', diagnostic)
                if not witness or 'load ' not in (ROOT/source).read_text().splitlines()[int(witness[1])-1]:
                    raise RuntimeError(f'{source}: missing load-site diagnostic\n{result.stdout}')
            print(diagnostic)
            refused += 1
    print(f'PASS AWM compiler preflight: {len(STARTUP)} ordinary modules, {refused} Dis64 refusals')


if __name__ == '__main__':
    main()
