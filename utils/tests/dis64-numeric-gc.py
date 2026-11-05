#!/usr/bin/env python3
"""Run actual numeric VM quanta with production collection between quanta."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Reuse the real execution closure and substitute a sweepable pool for its host allocations.
def unit(calls=False):
    execution = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-exec.py'))
    extract = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    source = execution['unit']().split('/* Execute an actual Frame/Modlink/decoder/VM/JIT program')[0]
    source = source.replace('int mutator, nprop, gchalt;', '')
    for signature in ('void\nmarkheap(', 'void*\npoolalloc(', 'void\npoolfree('):
        source = source.replace(extract(source, signature), '', 1)
    collector = (ROOT/'libinterp/gc.c').read_text()
    collector = '\n'.join(line for line in collector.splitlines() if not line.startswith('#include'))
    start = source.index('typedef struct Ptrhash')
    end = source.index('/* Native staging state is deliberately distinct', start)
    source = source[:start]+'''#define malloc(n) trackedallocate(n,0) // Track external root metadata.
'''+collector+'\n#undef malloc\n'+source[end:]
    fixture = (ROOT/'utils/tests/dis64-numeric-gc.c').read_text()
    if calls:
        stack = (ROOT/'libinterp/stack.c').read_text()
        xec = (ROOT/'libinterp/xec.c').read_text()
        for signature in ('void\nextend(', 'void\nunextend('):
            source += '\n'+extract(stack, signature)+'\n'
        for signature in ('static int\nmoduleentry(', 'static int\nframefits(',
                          'OP(mframe)', 'OP(mcall)', 'OP(ret)'):
            source += '\n'+extract(xec, signature)+'\n'
        fixture = fixture.split('/* Check C and machine-code execution with a standalone')[0]
        fixture += (ROOT/'utils/tests/dis64-provider-call.c').read_text()
        source += '\nString* c2string(char*, int);\n'
        inferno = (ROOT/'emu/port/inferno.c').read_text()
        clock = '\n'.join(extract(inferno, signature) for signature in
                          ('static void\nsysstatusframe(', 'void\nSys_millisec('))
        runt = (ROOT/'libinterp/runt.c').read_text()
        fixture = fixture.replace('CLOCK_HANDLER', clock)
        fixture = fixture.replace('CLOCK_RESOLVER', extract(runt, 'const Link*\nnative64sysclock('))
    return source+fixture


# Build independent paired images for native provider admission and refusal.
def provider_cases(fixtures, common):
    raw = fixtures['RAW']['RAW']
    binding, signature = raw['binding'], raw['signature']
    image, sections = fixtures['IMAGE'], fixtures['SECTIONS']

    def pair(import_args=None, export_args=None, **provider_args):
        imported = binding(**({'module': 0} | (import_args or {})))
        exported = binding(**({'export': True} | (export_args or {})))
        consumer = image(sections(name=5, imports=[imported]))
        provider = image(sections(**(common | {'exports': [exported]} | provider_args)))
        return consumer, provider

    yield 'match', pair()
    yield 'module', pair({'module': 3})
    yield 'symbol', pair({'symbol': 5})
    yield 'signature', pair({'sig': signature(1)})
    yield 'result', pair({'sig': signature(1)}, {'sig': signature(1)})
    yield 'parameters', pair({'sig': signature(0, (1,))}, {'sig': signature(0, (1,))})
    terminal = fixtures['EXIT']
    yield 'alternate-export', pair(
        export_args={'entry': 1, 'frame': 2},
        types=common['types']+[fixtures['D'](88, 8, b'\x00\xc0')],
        code=fixtures['W'](5)+terminal+common['code'][4:],
    )
    yield 'reserved-export-frame', pair(
        export_args={'frame': 2},
        types=common['types']+[fixtures['D'](80, 8, b'\x80\x00')],
    )
    yield 'nested-import', pair(imports=[binding()])
    yield 'no-export', pair(exports=[])
    yield 'ret-operand', pair(code=fixtures['W'](1)+fixtures['I']('IRET', 19, source=0))


# Keep the raw image independent and require the controlled missing trace to be detected.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', nargs='?', const='mp', choices=('mp', 'frame'))
    parser.add_argument('--provider', action='store_true')
    parser.add_argument('--calls', action='store_true')
    args = parser.parse_args()
    if sum(bool(option) for option in (args.provider, args.control, args.calls)) > 1:
        parser.error('select one execution mode')
    fixtures = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'))
    instruction = fixtures['I']('IADDW', 16, source=2, destination=8)
    words = fixtures['W']
    common = dict(
        data=words(1, 8, 1, 40),
        types=[fixtures['D'](80,8,b'\x00\xc0'), fixtures['D'](16,8,b'\x80')],
        code=words(4)+instruction*3+fixtures['EXIT'],
    )
    raw = fixtures['IMAGE'](fixtures['SECTIONS'](**common))
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    command = shlex.split(os.environ.get('CC', 'cc'))+flags+['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1']
    if args.calls:
        command += ['-DDIS_NATIVE64_BOOT_SYS_IO=1']
    if arch in ('arm64', 'riscv64'):
        command += ['-DDIS64_TEST_'+arch.upper()+'=1', '-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
    elif arch != 'amd64':
        raise ValueError('unsupported target '+arch)
    command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='numeric-gc-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        (work/'test.c').write_text(unit(args.calls))
        (work/'image').write_bytes(raw)
        binary = work/'test'
        subprocess.run(command+[str(work/'test.c'), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], check=True)
        runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
        if args.calls:
            for failure in (False, True):
                terminal = fixtures['I']('IDIVW', 16, source=0, destination=8) if failure else fixtures['I']('IRET', 27)
                pair = next(provider_cases(fixtures, common | {'code': words(3)+instruction*2+terminal}))[1]
                _, provider = pair
                binding = fixtures['RAW']['RAW']['binding']
                frame = fixtures['I']('IMFRAME', 65, source=0, middle=0, destination=64)
                call = fixtures['I']('IMCALL', 72, source=64, middle=0, destination=0)
                caller_code = words(3)+frame+call+fixtures['EXIT']
                caller = dict(name=5, databytes=8, imports=[binding(module=0)],
                              types=[fixtures['D'](80, 8), fixtures['D'](8, 8, b'\x80')],
                              code=caller_code)
                consumer = fixtures['IMAGE'](fixtures['SECTIONS'](**caller))
                (work/'consumer').write_bytes(consumer)
                (work/'image').write_bytes(provider)
                subprocess.run(runner+[str(binary), str(work/'image'), str(work/'consumer'),
                                       'error' if failure else 'return'], check=True, timeout=60)
            clock = runpy.run_path(str(ROOT/'utils/tests/dis64-pair-boot.py'))['clock_image'](repeats=2, copy=True)
            (work/'consumer').write_bytes(clock)
            subprocess.run(runner+[str(binary), str(work/'image'), str(work/'consumer'), 'clock'],
                           check=True, timeout=60)
            bad_frame = fixtures['I']('IMFRAME', 65, source=0, middle=0, destination=56)
            bad_call = fixtures['I']('IMCALL', 72, source=64, middle=1, destination=0)
            wrong_scratch = fixtures['I']('IMCALL', 72, source=72, middle=0, destination=0)
            refusals = [
                ('reserved-frame', {'code': words(3)+bad_frame+call+fixtures['EXIT']}),
                ('link-index', {'code': words(3)+frame+bad_call+fixtures['EXIT']}),
                ('scratch-mismatch', {'code': words(3)+frame+wrong_scratch+fixtures['EXIT']}),
                ('wrong-order', {'code': words(3)+call+frame+fixtures['EXIT']}),
                ('mapped-scratch', {'types': [fixtures['D'](80, 8, b'\x00\x80'), caller['types'][1]]}),
                ('scalar-mp', {'types': [caller['types'][0], fixtures['D'](8, 8)]}),
                ('import-limit', {'imports': [binding(module=0)]*33}),
                ('wrong-provider', {'imports': [binding(module=3)]}),
            ]
            for name, fields in refusals:
                (work/'consumer').write_bytes(fixtures['IMAGE'](fixtures['SECTIONS'](**(caller | fields))))
                mode = 'reject' if name == 'wrong-provider' else 'reject-early'
                result = subprocess.run(runner+[str(binary), str(work/'image'), str(work/'consumer'), mode],
                                        capture_output=True, text=True, timeout=60, check=True)
                print(result.stdout.strip(), '('+name+')')
            return
        if args.provider:
            for name, (consumer, provider) in provider_cases(fixtures, common):
                (work/'image').write_bytes(provider)
                (work/'consumer').write_bytes(consumer)
                mode = name if name in ('match', 'alternate-export') else 'reject'
                result = subprocess.run(
                    runner+[str(binary), str(work/'image'), str(work/'consumer'), mode],
                    capture_output=True, text=True, timeout=60,
                )
                if result.returncode:
                    raise RuntimeError(name+': '+result.stdout+result.stderr)
                print(result.stdout.strip(), '('+name+')')
            return
        result = subprocess.run(runner+[str(binary), str(work/'image')]+([args.control] if args.control else []), capture_output=True, text=True, timeout=60)
        if args.control:
            if result.returncode != 10 or result.stderr.strip() != 'numeric GC mismatch: '+args.control+' child was swept':
                raise RuntimeError('mapped trace control failed: '+str(result.returncode)+' '+result.stdout+result.stderr)
            print('PASS numeric GC trace control:', args.control, arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
