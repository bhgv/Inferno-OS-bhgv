#!/usr/bin/env python3
"""Compile the AWM startup slice and report the current Dis64 compiler barriers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import runpy
from collections import Counter
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STARTUP = (
    'appl/cmd/emuinit.b',
    'appl/wm/awm.b',
    'appl/wm/atoolbar.b',
    'appl/lib/arg.b',
    'appl/lib/bufio.b',
    'appl/lib/env.b',
    'appl/lib/filepat.b',
    'appl/lib/readdir.b',
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
def compile_module(limbo, source, target, options, env=None):
    return subprocess.run(
        [str(limbo), *options, '-I', 'module', '-o', str(target), source],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, timeout=60, env=env,
    )


# Installed file paths for the startup modules in this slice.
FILE_PROVIDERS = {
    '/dis/sh.dis': 'appl/cmd/sh/sh.b',
    **{f'/dis/lib/{Path(source).stem}.dis': source for source in STARTUP
       if source.startswith('appl/lib/')},
    **{f'/dis/wm/{Path(source).stem}.dis': source for source in STARTUP
       if source.startswith('appl/wm/')},
}


def assembly_imports(code):
    paths = dict(re.findall(r'^\s*string\s+@mp\+(\d+),"([^"]+)"$', code, re.M))
    groups = re.split(r'^\s*word\s+@ldt\+(\d+),(\d+)\s*$', code, flags=re.M)
    tables = {}
    for ordinal, count, body in zip(groups[1::3], groups[2::3], groups[3::3]):
        imports = re.findall(r'^\s*ext\s+@ldt\+\d+,(0x[0-9a-f]+),"([^"]+)"$', body, re.M)
        if len(imports) != int(count):
            raise RuntimeError(f'import table {ordinal} has the wrong size')
        tables[ordinal] = [(name, int(value, 16)) for value, name in imports]

    for offset, ordinal in re.findall(r'^\s*load\s+(\d+)\(mp\),\$(\d+),', code, re.M):
        path = paths.get(offset, '')
        # MP offsets in the assembly can be reused for non-module literals.
        if path.startswith(('$', '/dis/')):
            yield path, ordinal, tables[ordinal]


def check_startup_imports(limbo, work):
    """Check static LOAD records against real builtins and file exports."""
    config = (ROOT/'emu/Android/emu').read_text()
    mods = re.search(r'^mod\n((?:[ \t]+[^\n]*\n|\n)+)', config, re.M)
    if mods is None:
        raise RuntimeError('Android configuration has no mod section')
    builtins = {line.split()[0] for line in mods[1].splitlines() if line.strip()}
    assemblies = {}
    file_exports = {}
    for source in STARTUP:
        assembly = work/(Path(source).stem + '.s')
        result = compile_module(limbo, source, assembly, ['-S'])
        if result.returncode:
            raise RuntimeError(f'{source}: {result.stdout}')
        code = assembly.read_text()
        assemblies[source] = code
        file_exports[source] = {name: int(value, 16) for value, name in re.findall(
            r'^\s*link\s+[^,]+,[^,]+,(0x[0-9a-f]+),"([^"]+)"$', code, re.M)}

    native_exports = {}
    awm_paths = set()
    counts = {'builtin': 0, 'file': 0, 'self': 0}
    outside = set()
    groups = []
    for source, code in assemblies.items():
        for path, ordinal, imports in assembly_imports(code):
            group = {'source': source, 'path': path, 'ordinal': int(ordinal),
                     'symbols': [{'name': name, 'legacy_hash': f'{signature:08x}'}
                                 for name, signature in imports],
                     'legacy_names_hashes_match': False}
            groups.append(group)
            if path == '$self':
                exports = file_exports[source]
                kind = 'self'
            elif path.startswith('$'):
                module = path[1:]
                if module.lower() not in builtins:
                    raise RuntimeError(f'{source}: {path} is absent from the Android mod section')
                if path not in native_exports:
                    table = subprocess.check_output(
                        [str(limbo), '-t', module, '-I', 'module', 'module/runt.m'],
                        cwd=ROOT, text=True, timeout=60,
                    )
                    native_exports[path] = {name: int(value, 16) for name, value in re.findall(
                        r'^\s*"([^"]+)",(0x[0-9a-f]+),', table, re.M)}
                exports = native_exports[path]
                kind = 'builtin'
            elif path in FILE_PROVIDERS:
                exports = file_exports[FILE_PROVIDERS[path]]
                kind = 'file'
            else:
                outside.add(path)
                group['kind'] = 'outside-slice'
                continue
            for name, signature in imports:
                if exports.get(name) != signature:
                    raise RuntimeError(f'{source}: {path}->{name} does not match provider exports')
            counts[kind] += 1
            group.update(kind=kind, legacy_names_hashes_match=True)
            if kind == 'file':
                group['provider'] = FILE_PROVIDERS[path]
            elif kind == 'self':
                group['provider'] = source
            if source == 'appl/wm/awm.b':
                awm_paths.add(path)
                symbols = ', '.join(name for name, _ in imports)
                print(f'PASS AWM {path} import group {ordinal}: {symbols or "(empty)"}')
    if not {'$Sys', '$Draw', '/dis/lib/string.dis', '/dis/lib/wmclient.dis'} <= awm_paths:
        raise RuntimeError('AWM startup assembly is missing expected static LOAD records')
    print(f'PASS AWM startup slice: {counts["builtin"]} builtin, {counts["file"]} file and {counts["self"]} self import groups')
    if outside:
        print('Static LOAD paths outside the startup slice: ' + ', '.join(sorted(outside)))
    return {'groups': groups, 'verified_counts': counts, 'outside_paths': sorted(outside)}


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
# Checked native IR is a separate diagnostic route: its output must still be
# byte-for-byte classic Dis, and acceptance never implies file/VM admission.
def checked_frontier(limbo, source, work):
    env=dict(os.environ)
    for key in tuple(env):
        if key.startswith('LIMBO_DIS64_'):env.pop(key)
    plain=work/(Path(source).stem+'.checked-plain.dis')
    traced=work/(Path(source).stem+'.checked-traced.dis')
    baseline=compile_module(limbo,source,plain,['-i'],env)
    if baseline.returncode or any(marker in baseline.stdout for marker in ('AddressSanitizer','UndefinedBehaviorSanitizer','runtime error:')):raise RuntimeError(f'{source}: checked reference compile failed\n{baseline.stdout}')
    env.update(LIMBO_DIS64_TYPED='1',LIMBO_DIS64_TYPED_EXTERNAL='1',LIMBO_DIS64_TYPED_LOAD='1')
    result=compile_module(limbo,source,traced,['-i'],env)
    if result.returncode or any(marker in result.stdout for marker in ('AddressSanitizer','UndefinedBehaviorSanitizer','runtime error:')):
        raise RuntimeError(f'{source}: checked reporter failure\n{result.stdout}')
    data=plain.read_bytes()
    if traced.read_bytes()!=data:raise RuntimeError(f'{source}: diagnostic mode changed classic bytecode')
    rows=[json.loads(line[12:])for line in result.stdout.splitlines()if line.startswith('DIS64_TYPED ')]
    if not rows or len({(row['name'],row['first'])for row in rows})!=len(rows):raise RuntimeError(f'{source}: missing/duplicate function reports')
    reader=runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))
    code=reader['legacy_instructions'](data)[1]
    enumeration=(ROOT/'include/isa.h').read_text().split('};',1)[0]
    names=re.findall(r'^\s*(I[A-Z0-9]+)\s*,',enumeration,re.M)
    if not names or names[0]!='INOP':raise RuntimeError('unsupported opcode declaration syntax')
    functions=[];reasons=Counter()
    for row in rows:
        if type(row['accepted'])is not bool or not 0<=row['first']<len(code):raise RuntimeError(f'{source}: malformed frontier function')
        fn={k:row[k]for k in ('name','first','accepted','schema')}
        if row['accepted']:
            fn.update(instructions=len(row['code']),local_calls=len(row.get('calls',[])),imported_calls=len(row.get('externals',[])))
        else:
            reason=row['reason'];pc=row['pc'];fn.update(reason=reason,pc=pc);reasons[reason]+=1
            end=min((other['first']for other in rows if other['first']>row['first']),default=len(code))
            declaration_or_graph=reason in ('nonempty-output','function-result-or-polymorphism','aggregate-result','code-limit','storage-layout','multi-limit','multi-function-table','multi-instruction-table','multi-storage-budget','multi-depth','multi-cycle')
            if not declaration_or_graph and row['first']<=pc<end:
                ins=code[pc];fn['instruction']={**ins,'opcode':names[ins['op']]if ins['op']<len(names)else'unknown'}
                if fn['instruction']['opcode']in('IFRAME','IMFRAME'):
                    fn['following_instructions']=[{'pc':at,**code[at],'opcode':names[code[at]['op']]if code[at]['op']<len(names)else'unknown'}for at in range(pc+1,min(pc+5,end))]
            else:fn['location_note']='No instruction attributed: declaration/call-graph refusal or PC outside this body.'
        functions.append(fn)
    accepted=sum(fn['accepted']for fn in functions)
    print(f'PASS checked frontier: {source}: {accepted}/{len(functions)} bodies; classic bytes unchanged; refusals {dict(reasons)}')
    return {'schema':'inferno-checked-frontier-v1','diagnostic_only':True,'file_admission':False,
            'classic_bytes_unchanged':True,'classic_options':['-i'],'classic_sha256':hashlib.sha256(data).hexdigest(),
            'accepted_functions':accepted,'reasons':dict(reasons),'functions':functions}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--checked-only',action='store_true',help='report checked native IR functions and exact classic-byte preservation only')
    parser.add_argument('--report', type=Path, help='write compiler barriers and static LOAD dependencies as JSON')
    args = parser.parse_args()
    limbo = args.limbo.resolve()
    profiles = (("scalar", "-64"), ("managed", "-64l"), ("command-library", "-64b"), ("command-client", "-64c"))
    refused = {name: 0 for name, _ in profiles}
    report = {'schema': 'inferno-awm-preflight-v1',
              'scope': 'Compiler and static LOAD name/hash checks only; no runtime or Android acceptance.',
              'compiler_sha256': hashlib.sha256(limbo.read_bytes()).hexdigest(),
              'modules': [], 'execution_tested': False}
    with tempfile.TemporaryDirectory(prefix='awm-preflight-', dir=ROOT) as directory:
        work = Path(directory)
        if args.checked_only:
            report['scope']='Checked native IR diagnostics and classic byte preservation only; static imports, file profiles and runtime not checked.'
            for source in STARTUP:
                report['modules'].append({'source':source,'source_sha256':hashlib.sha256((ROOT/source).read_bytes()).hexdigest(),'checked_frontier':checked_frontier(limbo,source,work)})
            if args.report:args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
            print('PASS checked-only startup frontier: diagnostic evidence only; no AWM execution')
            return
        report['static_imports'] = check_startup_imports(limbo, work)
        check_string_frames(limbo, work)
        for source in STARTUP:
            legacy = work / (Path(source).stem + '.dis')
            result = compile_module(limbo, source, legacy, [])
            if result.returncode != 0 or not legacy.is_file() or legacy.stat().st_size == 0:
                raise RuntimeError(f'{source}: ordinary compilation failed\n{result.stdout}')
            module = {'source': source, 'source_sha256': hashlib.sha256((ROOT/source).read_bytes()).hexdigest(),
                      'classic': {'bytes': legacy.stat().st_size,
                                  'sha256': hashlib.sha256(legacy.read_bytes()).hexdigest()},
                      'profiles': {}, 'checked_frontier':checked_frontier(limbo,source,work)}
            report['modules'].append(module)
            for profile, option in profiles:
                target = work / (Path(source).stem + '.' + profile + '.dis64')
                result = compile_module(limbo, source, target, [option])
                if result.returncode == 0:
                    if not target.is_file() or target.stat().st_size == 0:
                        raise RuntimeError(f'{source}: {profile} compiler omitted its output')
                    print(f'{source}: emitted {profile} Dis64 (compiler evidence only)')
                    module['profiles'][profile] = {'option': option, 'emitted': True,
                                                  'bytes': target.stat().st_size}
                    continue
                diagnostic = next((line for line in result.stdout.splitlines() if 'Dis64 ' in line), '')
                if result.returncode < 0 or (target.exists() and target.stat().st_size) or not diagnostic or "AddressSanitizer" in result.stdout or "runtime error:" in result.stdout:
                    raise RuntimeError(f'{source}: unexpected {profile} refusal\n{result.stdout}')
                if profile == "scalar" and source == 'appl/wm/awm.b' and 'module load requires managed linkage' in diagnostic:
                    witness = re.search(r':(\d+): Dis64 scalar: module load requires managed linkage', diagnostic)
                    if not witness or 'load ' not in (ROOT/source).read_text().splitlines()[int(witness[1])-1]:
                        raise RuntimeError(f'{source}: missing load-site diagnostic\n{result.stdout}')
                print(f'[{profile}] {diagnostic}')
                module['profiles'][profile] = {'option': option, 'emitted': False,
                                              'diagnostic': diagnostic}
                refused[profile] += 1
    report['refused'] = refused
    if args.report:
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(f'PASS AWM compiler preflight: {len(STARTUP)} ordinary modules, {refused["scalar"]} scalar and {refused["managed"]} managed, {refused["command-library"]} command-library and {refused["command-client"]} command-client Dis64 refusals; no AWM execution claimed')



if __name__ == '__main__':
    main()
