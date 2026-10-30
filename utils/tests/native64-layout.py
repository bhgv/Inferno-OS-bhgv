#!/usr/bin/env python3
"""Generate experimental native64 metadata with real Limbo and check actual C/map8."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Distinguish compiler/runtime failures from deliberately unsupported metadata requests.
def run(command, work, env, success=True):
    result = subprocess.run(list(map(str, command)), cwd=work, env=env, capture_output=True, text=True)
    if (result.returncode == 0) != success:
        raise RuntimeError(shlex.join(list(map(str, command)))+'\n'+result.stdout+result.stderr)
    return result


# Build a native tool unless the caller explicitly supplies an already rebuilt executable.
def main():
    env = dict(os.environ)
    env['CFLAGS'] = os.environ.get('NATIVE64_CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all')
    with tempfile.TemporaryDirectory(prefix='native64 declarations ') as temporary:
        work = Path(temporary)
        if env.get('NATIVE64_LIMBO'):
            limbo = Path(env['NATIVE64_LIMBO']).resolve()
        else:
            tools = work/'tools'
            buildenv = dict(env, INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_LIMBO='1', MKBOOTSTRAP_BINDIR=str(tools))
            run(['sh', ROOT/'makemk.sh'], work, buildenv)
            limbo = tools/'limbo'
        args = ', '.join('a'+str(i)+': string' for i in range(150))
        source = work/'probe.m'
        source.write_text('''Probe: module {
    Outer: adt { inner: Inner; lead: byte; link: string; ints: Pair; };
    Inner: adt { count: int; name: string; x: big; };
    Tail: adt { value: int; next: ref Tail; };
    Pair: adt { a, b: int; };
    Byte4: adt { a, b, c, d: byte; };
    Tuple: type (int, string, big);
    Nested: adt { tag: byte; pair: (int, string); tail: int; };
    Reply: type chan of (int, string);
    Work: adt { out: chan of (int, ref Tail); };
    value: fn(x: int, y: string, z: Outer): (int, ref Tail);
    noop: fn();
    vari: fn(fmt: string, *): int;
    many: fn('''+args+''');
};
''')
        header = work/'native64.h'
        generated = run([limbo, '-H', source], work, env).stdout
        if generated != run([limbo, '-H', source], work, env).stdout:
            raise RuntimeError('nondeterministic native64 declarations')
        header.write_text(generated)
        if list(work.glob('*.dis')) or list(work.glob('*.sbl')):
            raise RuntimeError('metadata-only generation created bytecode or symbols')
        compiler = shlex.split(env.get('CC', 'cc'))
        flags = shlex.split(env['CFLAGS'])
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include')]
        for label, selection in (
            ('WORD32', ['-DDIS_WORD32=1']),
            ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1']),
            ('legacy-rejected', ['-DNATIVE64_UNSUPPORTED']),
            ('packed-rejected', ['-DDIS_WORD32=1', '-DNATIVE64_UNSUPPORTED', '-fpack-struct=1']),
        ):
            binary = work/label
            run(compiler+flags+selection+includes+[ROOT/'utils/tests/native64-layout.c', '-o', binary], work, env)
            print(run([binary], work, env).stdout.strip(), flush=True)
        sysheader = work/'sys.h'
        sysheader.write_text(run([limbo, '-H', ROOT/'module/sys.m'], work, env).stdout)
        report = run([sys.executable, ROOT/'libinterp/tests/native-consumer-layout.py', '--header', sysheader,
                      '--require-coherent'], work, env)
        if not json.loads(report.stdout)['representative_checks_match'] or json.loads(report.stdout)['full_native_abi_verified']:
            raise RuntimeError('representative metadata was confused with full native ABI acceptance')
        # Exercise all emitted Sys/Math fields and frames, not just representative offsets.
        for name in ('sys', 'math'):
            complete = run([limbo, '-H', ROOT/('module/'+name+'.m')], work, env).stdout
            selected = work/(name+'-complete.h')
            selected.write_text(complete)
            probe = work/(name+'-complete.c')
            probe.write_text('#include "lib9.h"\n#include "isa.h"\n#include "interp.h"\n#include "'+selected.name+'"\nint main(void) { return limbo_native64_'+name.capitalize()+'_layout() ? 0 : 1; }\n')
            binary = work/(name+'-complete')
            run(compiler+flags+['-DDIS_WORD32=1']+includes+[probe, '-o', binary], work, env)
            run([binary], work, env)
        combined = work/'combined.c'
        combined.write_text('#include "lib9.h"\n#include "isa.h"\n#include "interp.h"\n#include "sys-complete.h"\n#include "math-complete.h"\nint main(void) { return limbo_native64_Sys_layout() && limbo_native64_Math_layout() ? 0 : 1; }\n')
        binary = work/'combined'
        run(compiler+flags+['-DDIS_WORD32=1']+includes+[combined, '-o', binary], work, env)
        run([binary], work, env)
        # Alter an actual named field while preserving its size and all aggregate extents.
        if 'WORD a; // Offset 0, 4 bytes.' not in generated:
            raise RuntimeError('missing concrete Pair mutation target')
        damaged = generated.replace('WORD a; // Offset 0, 4 bytes.', 'WORD b; // Offset 0, 4 bytes.', 1)
        damaged = damaged.replace('WORD b; // Offset 4, 4 bytes.', 'WORD a; // Offset 4, 4 bytes.', 1)
        header.write_text(damaged)
        negative = work/'layout-negative.c'
        negative.write_text('#include "lib9.h"\n#include "isa.h"\n#include "interp.h"\n#include "native64.h"\nint main(void) { return limbo_native64_model() && !limbo_native64_Probe_layout() ? 0 : 1; }\n')
        binary = work/'layout-negative'
        run(compiler+flags+['-DDIS_WORD32=1']+includes+[negative, '-o', binary], work, env)
        run([binary], work, env)
        header.write_text(generated)
        for name in ('sys', 'math'):
            baseline = run([limbo, '-a', ROOT/('module/'+name+'.m')], work, env).stdout
            if baseline != run([limbo, '-a', ROOT/('module/'+name+'.m')], work, env).stdout:
                raise RuntimeError('legacy declaration output changed between runs')
        legacy = work/'legacy-sys.h'
        legacy.write_text(run([limbo, '-a', ROOT/'module/sys.m'], work, env).stdout)
        result = subprocess.run([sys.executable, str(ROOT/'libinterp/tests/native-consumer-layout.py'), '--header', str(legacy),
                                 '--require-coherent'], cwd=work, env=env, capture_output=True, text=True)
        if result.returncode != 2:
            raise RuntimeError('legacy native layout must remain a measured incompatibility')
        bad = [(['-H', '-S', source], 'usage'), (['-H', '-t', 'Probe', source], 'usage'),
               (['-H', '-A', source], 'usage'), (['-H', '-o', work/'forbidden', source], 'usage'),
               (['-H', source, source], 'usage')]
        for command, diagnostic in bad:
            result = run([limbo]+command, work, env, False)
            if result.stdout or diagnostic not in result.stderr:
                raise RuntimeError('invalid metadata selection published output')
        if (work/'forbidden').exists():
            raise RuntimeError('invalid selection created its output file')
        deep = 'Probe: module {'+' '.join('T'+str(i)+': adt { x: '+('T'+str(i+1) if i<144 else 'int')+'; };' for i in range(145))+'};'
        for name, text, diagnostic in (
            ('empty', 'Probe: module { Empty: adt {}; };', 'empty aggregates'),
            ('polymorphic', 'Probe: module { f: fn[T](x: T): T; };', 'polymorphic'),
            ('reserved', 'Probe: module { f: fn(ret: int): int; };', 'argument name'),
            ('reservedfield', 'Probe: module { T: adt { __limbo_native64_pad1: int; }; };', 'reserved metadata prefix'),
            ('picked', 'Probe: module { T: adt { pick { A => x: int; B => y: string; } }; };', 'picked types'),
            ('deep', deep, 'nested too deeply'),
            ('moduledata', 'Probe: module { T: adt { x: int; }; item: ref T; };', 'module data'),
            ('collision', 'A_B: module { C: adt { x: int; }; }; A: module { B_C: adt { x: int; }; };', 'C name collision'),
            ('keyword', 'Probe: module { T: adt { register: int; }; };', 'C keyword'),
        ):
            invalid = work/(name+'.m')
            invalid.write_text(text)
            result = run([limbo, '-H', invalid], work, env, False)
            if result.stdout or diagnostic not in result.stderr:
                raise RuntimeError('unsupported '+name+' metadata was not rejected before publication\n'+result.stdout+result.stderr)
        print('PASS native64: real compiler, 2 compatible/2 rejected models, Sys/Math complete geometry, runtime offset negative control, legacy unchanged model, deterministic output and 14 preflight/CLI rejections', flush=True)


if __name__ == '__main__':
    main()
