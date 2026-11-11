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
def unit(calls=False, managed=False):
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
    if managed:
        fixture = fixture.split('/* Run single-instruction quanta')[0]
        # A reload retains the previous instance until the new one is published; list input adds two nodes.
        fixture = fixture.replace('#define GcSlots 8', '#define GcSlots 16')
        source = source.replace('#include "tests/leaf-cons-stub.h"',
                                '#define cons fixture_leaf_cons\n#include "tests/leaf-cons-stub.h"\n#undef cons')
        heap = (ROOT/'libinterp/heap.c').read_text()
        source += '\n'.join(extract(heap, signature) for signature in
                            ('void\nfreelist(', 'void\nfreestring('))
        source += '\nType Tlist = {1, freelist, marklist, sizeof(List)};\n'
        source += extract((ROOT/'libinterp/xec.c').read_text(), 'List*\ncons(')+'\n'
        source += '#include "dis64-managed-signature.h"\n'
        source += '#include "dis64-managed-trial.h"\n'
        load64 = (ROOT/'libinterp/load64.c').read_text()
        link = (ROOT/'libinterp/link.c').read_text()
        source = source.replace(extract(source, 'void\ndestroylinks('), extract(link, 'void\ndestroylinks('), 1)
        source += '\n#define malloc(n) trackedallocate(n,0)\n'+extract(link, 'static void\nnewlink(')+'\n'+extract(link, 'void\nmlink(')+'\n#undef malloc\n'
        source += '#include "string-hashes.h"\nint dis64bootmanaged; char *dis64bootprovider;\n'
        source += r'''/* ASCII filename allocator only; complete UTF conversion runs in console tests. */
String* c2string(char *text, int length)
{
    Heap *heap;
    String *value;
    int i;
    require(length >= 0);
    for(i=0; i<length; i++) require((uchar)text[i] < 128);
    heap = nheap(sizeof(String)+length);
    heap->t = &Tstring;
    Tstring.ref++;
    value = H2D(String*, heap);
    value->tmp = nil;
    value->len = value->max = length;
    memmove(value->Sascii, text, length);
    value->Sascii[length] = 0;
    return value;
}
'''

        begin = load64.index('#ifdef DIS_NATIVE64_MANAGED_FILES\n/* Generated')
        end = load64.index('/* Only the explicit console option', begin)
        source += load64[begin:end]
        load = (ROOT/'libinterp/load.c').read_text()
        readmod = (ROOT/'libinterp/readmod.c').read_text()
        source += '\nint cflag, dontcompile;\n'+extract((ROOT/'emu/port/dis.c').read_text(), 'void\nkrethrow(')+'\n'
        source += load[load.index('typedef struct DisLoad'):load.index('#if defined(DIS64_PILOT_LOADER)', load.index('typedef struct DisLoad'))]
        source += extract(load, 'static int\ndis64parse(')+'\n'+extract(load, 'Module*\nlookmod(')+'\n'
        source += '#define snprint snprintf\n'+extract(link, 'static int\nlinkm(')+'\n#undef snprint\n'
        source += link[link.index('typedef struct LinkModule'):link.index('/* Preflight MP descriptors')]
        source += extract(link, 'static int\nlinkmodule(')+'\n'+extract(link, 'Modlink*\nlinkmod(')+'\n'
        source += '#define DIS_NATIVE64_SCALAR_BOOT 1\n'+extract(readmod, 'Module*\nreadmod(')+'\n#undef DIS_NATIVE64_SCALAR_BOOT\n'
        source += '\n'.join(extract(load64, signature) for signature in
                            ('static Modlink*\ndis64managedowner(', 'Modlink*\nprepare64managedtrial('))
        source += '\n'
        fixture += (ROOT/'utils/tests/dis64-managed-gc.c').read_text()+'\n'+(ROOT/'utils/tests/dis64-managed-load.c').read_text()
    if calls or managed:
        stack = (ROOT/'libinterp/stack.c').read_text()
        xec = (ROOT/'libinterp/xec.c').read_text()
        for signature in ('void\nextend(', 'void\nunextend('):
            source += '\n'+extract(stack, signature)+'\n'
        for signature in ('static int\nmoduleentry(', 'static int\nframefits(',
                          'OP(mframe)', 'OP(mcall)', 'OP(ret)'):
            source += '\n'+extract(xec, signature)+'\n'
    if managed:
        for signature in ('static Import*\nmoduleimports(', 'OP(iload)', 'OP(movp)', 'OP(consp)', 'static List*\nlisthead(', 'OP(headp)'):
            source += '\n'+extract(xec, signature)+'\n'
    if calls:
        fixture = fixture.split('/* Check C and machine-code execution with a standalone')[0]
        fixture += (ROOT/'utils/tests/dis64-provider-call.c').read_text()
        source += '\nString* c2string(char*, int);\n'
        inferno = (ROOT/'emu/port/inferno.c').read_text()
        clock = '\n'.join(extract(inferno, signature) for signature in
                          ('static void\nsysstatusframe(', 'void\nSys_millisec('))
        runt = (ROOT/'libinterp/runt.c').read_text()
        fixture = fixture.replace('CLOCK_HANDLER', clock)
        resolver = '\n'.join(extract(runt, signature) for signature in
                             ('const Link*\nnative64sysexport(', 'const Link*\nnative64sysclock('))
        fixture = fixture.replace('CLOCK_RESOLVER', resolver)
    if managed:
        source = '#include "dis64-managed-trial.h"\nextern int dis64bootmanaged; extern char *dis64bootprovider;\n'+source
    source = source.replace('require(int condition)', 'requireline(int condition, int line)', 1)
    source = source.replace('failed after %d predicates\\n", checks);', 'failed after %d predicates at line %d\\n", checks, line);', 1)
    source = source.replace('/* Bound each allocation', '#define require(condition) requireline((condition), __LINE__)\n\n/* Bound each allocation', 1)
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


# Build complete trial images with independent Python envelope and record writers.
def managed_image_cases(fixtures, load_path=None, reloads=()):
    words, desc = fixtures['W'], fixtures['D']
    image, sections = fixtures['IMAGE'], fixtures['SECTIONS']
    binding = fixtures['RAW']['RAW']['binding']
    signatures = (words(0xface0010, 1, 0x72, 0x71, 0x71),
                  words(0xface0010, 1, 0x71, 0x72, 0x71))
    symbols = ('unquoted', 'quoted')
    indirect = runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    rows = []

    def build(names, **fields):
        pool = words(len(names))+b''.join(words(len(name), *map(ord, name)) for name in names)
        return image([(kind, pool if kind == 7 else body) for kind, body in sections(**fields)])

    for direction, signature in enumerate(signatures):
        symbol = symbols[direction]
        opcode = ('ICONSP', 'IHEADP')[direction]
        terminal = fixtures['I']('IRET', 27)
        body = indirect(opcode, 64, 32, 0)+terminal
        ins = fixtures['I']
        def caller(*, frame_map=b'\x00\xc0', mp_map=None, argument=64, target=72,
                   repeats=1, clear_source=8, clear_target=72, clear_mode=1, cleanup=True, mp_bytes=None,
                   reload_source=16, reload_target=0, reload_binding=0, duplicate_reload=False, load_before_clear=False, initial_load=True,
                   trailing_load=False, load_in_call=False):
            call = (ins('IMFRAME', 65, source=0, middle=0, destination=40)
                    + indirect('IMOVP', argument, 40, 64)
                    + indirect('ILEA', target, 40, 32)
                    + ins('IMCALL', 72, source=40, middle=0, destination=0))
            if load_in_call:
                frame_bytes = len(ins('IMFRAME', 65, source=0, middle=0, destination=40))
                call = call[:frame_bytes]+ins('ILOAD', 64, source=16, middle=0, destination=0)+call[frame_bytes:]
            clear = ins('IMOVP', clear_mode, source=clear_source, destination=clear_target) if cleanup else b''
            fileload = load_path is not None
            code, count = b'', 1
            for index in range(repeats):
                reload = index > 0 and (reloads is True or index in reloads)
                load = ins('ILOAD', 64, source=16 if index == 0 else reload_source,
                           middle=0 if index == 0 else reload_binding,
                           destination=0 if index == 0 else reload_target) if (fileload and index == 0 and initial_load) or reload else b''
                if duplicate_reload and reload:
                    load += load
                cleanup_code = clear if index else b''
                code += (load+cleanup_code if load_before_clear else cleanup_code+load)+call
                count += 4+int(load_in_call)+int(bool(cleanup_code))+int(bool(load))+(int(duplicate_reload and reload))
            if trailing_load:
                code += clear+ins('ILOAD', 64, source=16, middle=0, destination=0)
                count += 1+int(bool(clear))
            code += fixtures['EXIT']
            mp_bytes = (24 if fileload else 8 if repeats == 1 else 16) if mp_bytes is None else mp_bytes
            mp_map = (b'\xe0' if fileload else b'\x80' if repeats == 1 else b'\xc0') if mp_map is None else mp_map
            names = ('caller', symbol, 'String', load_path) if fileload else ('caller', symbol, 'String')
            return build(names, name=0, databytes=mp_bytes, code=words(count)+code,
                         types=[desc(80, 8, frame_map), desc(mp_bytes, 8, mp_map)],
                         imports=[binding(module=2, symbol=1, sig=signature)])

        consumer = caller()
        def provider(*, name='String', export_name=symbol, sig=signature, entry=1,
                     frame_map=b'\x00\x80', duplicate=False, code=body,
                     prefix=fixtures['EXIT']):
            export = binding(symbol=2, export=True, entry=entry, frame=0, sig=sig)
            return build((name, 'unused', export_name), name=0, profile=0, databytes=0,
                         code=words(3)+prefix+code, types=[desc(72, 8, frame_map)],
                         exports=[export]*(2 if duplicate else 1))

        good = provider()
        rows.extend((direction, accepted, consumer, peer) for accepted, peer in (
            (1, good), (0, good[:-1]), (0, provider(name='Other')),
            (0, provider(export_name=symbols[1-direction])),
            (0, provider(sig=signatures[1-direction])), (0, provider(entry=2)),
            (0, provider(frame_map=b'\x00\x00')), (0, provider(duplicate=True)),
            (0, provider(code=indirect(('IHEADP', 'ICONSP')[direction], 64, 32, 0)+terminal)),
            (0, provider(code=indirect(opcode, 64, 32, 8)+terminal)),
            (0, provider(prefix=terminal)), (0, provider(code=body+terminal))))
        rows.extend((direction, 0, bad, good) for bad in (
            caller(frame_map=b'\x00\x40'), caller(mp_map=b'\x00'),
            caller(argument=72), caller(target=64)))
        rows.extend((direction, 1, caller(repeats=n), good) for n in (2, 3, 32))
        rows.extend((direction, 0, bad, good) for bad in (
            caller(repeats=33), caller(repeats=2, cleanup=False),
            caller(repeats=2, clear_source=0), caller(repeats=2, clear_target=64),
            caller(repeats=2, clear_target=40), caller(repeats=2, clear_mode=9),
            caller(repeats=2, mp_map=b'\x80'), caller(repeats=2, mp_bytes=8),
            caller(mp_bytes=16, mp_map=b'\xc0')))
        if reloads:
            rows.extend((direction, 0, bad, good) for bad in (
                caller(repeats=2, reload_source=8), caller(repeats=2, reload_target=8),
                caller(repeats=2, reload_binding=1), caller(repeats=2, duplicate_reload=True),
                caller(repeats=2, load_before_clear=True), caller(repeats=2, initial_load=False),
                caller(repeats=2, trailing_load=True), caller(repeats=2, load_in_call=True)))
    return rows


def managed_sources(direction):
    symbol = 'quoted' if direction else 'unquoted'
    argument = 'list of string' if direction else 'string'
    result = 'string' if direction else 'list of string'
    expression = 'hd value' if direction else 'value :: nil'
    provider = (f'implement String;\nString: module {{ {symbol}: fn(value: {argument}): {result}; }};\n'
                f'{symbol}(value: {argument}): {result} {{ return {expression}; }}\n')
    caller = ('implement Caller;\ninclude "string.m";\n'
              f'Caller: module {{ init: fn(value: {argument}); }};\n'
              f'init(value: {argument}) {{\n'
              '    lib := load String String->PATH;\n'
              f'    result := lib->{symbol}(value);\n}}\n')
    return caller, provider


def managed_images(fixtures, limbo, work):
    rows = managed_image_cases(fixtures)
    for direction in (0, 1):
        for repetitions in (1, 32):
            compiled = []
            for label, text in zip(('caller', 'provider'), managed_sources(direction)):
                if label == 'caller' and repetitions != 1:
                    symbol = 'quoted' if direction else 'unquoted'
                    statement = f'    result := lib->{symbol}(value);'
                    text = text.replace(statement, statement+'\n'+f'    result = lib->{symbol}(value);\n'*(repetitions-1))
                source = work/f'{label}-{direction}-{repetitions}.b'
                output = source.with_suffix('.dis64')
                source.write_text(text)
                subprocess.run([str(limbo.resolve()), '-64m', '-I', str(ROOT/'module'),
                                '-o', str(output), str(source)], check=True, timeout=60)
                compiled.append(output.read_bytes())
            rows.append((direction, 1, *compiled))
    rows.extend(managed_image_cases(fixtures, '/managed-provider.dis64'))
    rows.extend(managed_image_cases(fixtures, '/managed-provider.dis64', reloads=True))
    # The real cached-load/GC execution uses compiler-produced repeated file LOADs, too.
    for direction in (0, 1):
        caller, provider = managed_sources(direction)
        symbol = 'quoted' if direction else 'unquoted'
        path = '/managed-provider.dis64'
        statement = f'    result := lib->{symbol}(value);'
        reload_line = '    lib = load String "'+path+'";'
        caller = caller.replace('String->PATH', '"'+path+'"').replace(statement,
            statement+'\n'+(reload_line+'\n'+f'    result = lib->{symbol}(value);\n')*31)
        compiled = []
        for label, text, flag in (('caller', caller, '-64l'), ('provider', provider, '-64m')):
            source = work/f'cached-{label}-{direction}.b'
            output = source.with_suffix('.dis64')
            source.write_text(text)
            subprocess.run([str(limbo.resolve()), flag, '-I', str(ROOT/'module'),
                            '-o', str(output), str(source)], check=True, timeout=60)
            compiled.append(output.read_bytes())
        rows.append((direction, 1, *compiled))
    good_providers = {provider for _, accepted, _, provider in rows if accepted}
    declarations, entries = [], []
    for index, (direction, accepted, consumer, provider) in enumerate(rows):
        for label, data in (('consumer', consumer), ('provider', provider)):
            declarations.append(f'static const uchar {label}{index}[] = {{'+','.join(map(str, data))+'};')
        entries.append(f'{{{direction},{accepted},{int(provider in good_providers)},consumer{index},sizeof(consumer{index}),'
                       f'provider{index},sizeof(provider{index})}}')
    return '\n'.join(declarations)+'\nstatic const ManagedImageCase managedimages[] = {\n'+',\n'.join(entries)+'\n};\n'


# Keep the raw image independent and require the controlled missing trace to be detected.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', nargs='?', const='mp', choices=('mp', 'frame'))
    parser.add_argument('--provider', action='store_true')
    parser.add_argument('--calls', action='store_true')
    parser.add_argument('--managed', action='store_true')
    parser.add_argument('--limbo', type=Path)
    args = parser.parse_args()
    if sum(bool(option) for option in (args.provider, args.control, args.calls, args.managed)) > 1:
        parser.error('select one execution mode')
    if args.managed and args.limbo is None:
        parser.error('--managed requires --limbo to generate the String frames')
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
    if args.managed:
        command += ['-DDIS_NATIVE64_MANAGED_FILES=1']
    if arch in ('arm64', 'riscv64'):
        command += ['-DDIS64_TEST_'+arch.upper()+'=1', '-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
    elif arch != 'amd64':
        raise ValueError('unsupported target '+arch)
    command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='numeric-gc-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        if args.managed:
            (work/'managed-images.h').write_text(managed_images(fixtures, args.limbo, work))
            (work/'string-native64.h').write_bytes(subprocess.check_output(
                [str(args.limbo.resolve()), '-H', '-M', 'String', '-I'+str(ROOT/'module'),
                 str(ROOT/'module/string.m')], timeout=60))
            header = (work/'string-native64.h').read_text()
            (work/'string-hashes.h').write_text('\n'.join(line for line in header.splitlines()
                if line.startswith(('#define LIMBO_NATIVE64_SIGNATURE_String_quoted ',
                                    '#define LIMBO_NATIVE64_SIGNATURE_String_unquoted ')))+'\n')
            probe = work/'unsupported.m'
            probe.write_text('Unsupported: module { wrong: fn(s: list of int): string; '
                             'same: fn(s: string): string; };\n')
            unsupported = subprocess.check_output([str(args.limbo.resolve()), '-H', str(probe)],
                                                  timeout=60)
            if b'limbo_native64_Unsupported_wrong_managed' in unsupported or \
               b'limbo_native64_Unsupported_same_managed' in unsupported:
                raise RuntimeError('Limbo emitted a managed witness for an unsupported type')
        (work/'test.c').write_text(unit(args.calls, args.managed))
        (work/'image').write_bytes(raw)
        binary = work/'test'
        subprocess.run(command+[str(work/'test.c'), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], check=True)
        runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
        if args.managed:
            subprocess.run(runner+[str(binary)], check=True, timeout=60)
            return
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
