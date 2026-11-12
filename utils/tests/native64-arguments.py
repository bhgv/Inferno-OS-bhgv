#!/usr/bin/env python3
"""Build and retire actual managed typed-argument contexts through both protected port boundaries."""
from pathlib import Path
import argparse
import os
import resource
import runpy
import shlex
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Bound only native allocation/process services; retain real heap, collector, Type and error statements.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--formatter', action='store_true', help='Also exercise the actual shared formatter with typed slots and real lib9')
    parser.add_argument('--android-ndk', type=Path, help='build typed formatter fixtures against ARM64 Bionic with NDK clang')
    args = parser.parse_args()
    if args.android_ndk and not args.formatter:
        parser.error('--android-ndk currently requires --formatter')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-result.py'))
    function = support['function']
    env = dict(os.environ)
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    default_flags = '-O3 -g -DNDEBUG -fno-strict-aliasing -static' if args.android_ndk else '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'
    flags = shlex.split(env.get('NATIVE64_CFLAGS', default_flags))
    with tempfile.TemporaryDirectory(prefix='native64-arguments-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        limbo = args.limbo.resolve()
        roots = work/'roots.m'
        roots.write_text('Roots: module { Node: adt { next: ref Node; }; copy: fn(source: array of ref Node): array of ref Node; caller: fn(padding: big, result: array of byte, tail: array of byte); bridge: fn(count: int, canary: int, stringresult: string, arrayresult: array of byte, text: string); many: fn('+','.join('a'+str(i)+': array of byte' for i in range(150))+'); };')
        for name, source in [('math', ROOT/'module/math.m'), ('sys', ROOT/'module/sys.m'), ('roots', roots)]:
            (work/(name+'-native64.h')).write_bytes(subprocess.check_output([str(limbo), '-H', str(source)], cwd=work, env=env))
        if args.formatter:
            runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))['generate'](limbo, work, env)
        if args.android_ndk:
            clang = args.android_ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
            if not clang.is_file(): parser.error('Missing NDK ARM64 clang: '+str(clang))
            compiler = [str(clang)]
        else:
            compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        if args.android_ndk:
            includes[0:0] = ['-I'+str(ROOT/p) for p in ('Android/arm64/include', 'Android/arm/include')]
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])]:
            command = compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections']
            objects = []
            for name in ('heap', 'math'):
                obj = work/(label+'-'+name+'.o')
                options = ['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"'] if name == 'math' else []
                subprocess.run(command+options+['-c', str(ROOT/('libinterp/'+name+'.c')), '-o', str(obj)], env=env, check=True)
                objects.append(str(obj))
            for port in ('emu', 'os'):
                source = support['unit'](port).replace('#define SlotBytes 256', '#define SlotBytes 2048')
                source = source.replace('int\nmain(void)', 'int\nnative64_original_result_main(void)', 1)
                source = source.replace(function(source, 'void*\nmallocz('), 'void *mallocz(ulong, int);', 1)
                oldpool = function(source, 'void*\npoolalloc(')
                newpool = oldpool.replace('\tcheck(pool == heapmem', '\tif(contextheapfail) return nil;\n\tcheck(pool == heapmem', 1)
                newpool = newpool.replace('\t\tcheck(freed[index] == 0);', '\t\tif(sourcebankpool) { freed[index] = 0; sourcebankallocs++; sourcebanklive++; }\n\t\tcheck(freed[index] == 0);')
                source = source.replace(oldpool, 'static int contextheapfail; // Fail only the actual heapz pool boundary.\n\n'+newpool, 1)
                oldfree = function(source, 'void\npoolfree(')
                newfree = oldfree.replace('\tfreed[index]++;', '\tfreed[index]++;\n\tif(sourcebankpool) { sourcebankfrees++; sourcebanklive--; check(sourcebanklive >= 0); }')
                source = source.replace(oldfree, 'static int sourcebankpool, sourcebankallocs, sourcebankfrees, sourcebanklive; // Per-generation transport ownership.\n'+newfree, 1)
                unit = work/(port+'.c')
                source += (ROOT/'utils/tests/native64-arguments.c').read_text()
                extra = []
                if args.formatter:
                    runtime = (ROOT/'libinterp/runt.c').read_text()
                    a = runtime.index('typedef struct XprintString')
                    b = runtime.index('/* Validate borrowed string', a)
                    core = runtime[a:b]
                    a = runtime.index('/* Private reader categories')
                    b = runtime.index('/* Preserve packed legacy strides', a)
                    core += runtime[a:b]
                    for signature in ('static int\nxsnprint(', 'static void\nxprintstring(', 'static uchar*\nxprintargument(', 'static void\nxprinttake(', 'static int\nxprintwithargs('):
                        core += '\n'+function(runtime, signature)+'\n'
                    strings = (ROOT/'libinterp/string.c').read_text()
                    core += '\n'+function(strings, 'String*\nnewstring(')+'\n'
                    core += '\n'+function(strings, 'String*\nnewrunes(')+'\n'
                    core += '\n'+function(strings, 'String*\nc2string(')+'\n'
                    core += '\n'+function(runtime, 'Array*\nmem2array(')+'\n'
                    source = source.replace('int\nmain(int argc, char **argv)', 'int\nnative64_arguments_main(int argc, char **argv)', 1)
                    source += '\nchar exInval[]="invalid", exBounds[]="bounds"; // Actual formatter error categories.\n'
                    source += 'char *syserr(char*, char*, Prog*);\nint hmsize(void*);\n'+core
                    source += (ROOT/'utils/tests/native64-format.c').read_text().replace('int\nmain(int argc, char **argv)', 'int\nnative64_formatter_main(int argc, char **argv)', 1)
                    source += (ROOT/'utils/tests/native64-sys-format.c').read_text().replace('int\nmain(int argc, char **argv)', 'int\nnative64_sysformat_main(int argc, char **argv)', 1)
                    source += (ROOT/'utils/tests/native64-sys-write.c').read_text().replace('int\nmain(int argc, char **argv)', 'int\nnative64_syswrite_main(int argc, char **argv)', 1)
                    vm = (ROOT/'libinterp/xec.c').read_text()
                    bodies = '#define OP(fn) void fn(void)\n'+'\n'.join(function(vm, signature) for signature in ('static Type*\nmoduletype(', 'static int\nmoduleentry(', 'static int\nframefits(', 'OP(frame)', 'OP(mframe)', 'OP(mcall)', 'OP(movw)', 'OP(movp)', 'OP(movl)', 'OP(movf)', 'OP(cvtwl)', 'OP(cvtwf)', 'OP(cvtlf)'))
                    source += '\n'+function((ROOT/'libinterp/load.c').read_text(), 'void\nfreemod(').replace('void\nfreemod(', 'void\nnative64_owned_freemod(', 1)+'\n'
                    source += (ROOT/'utils/tests/native64-sys-frame.c').read_text().replace('VM_FRAME_IMPLEMENTATION', bodies)
                    for signature in ('int\nSys_sprint64(', 'int\nSys_aprint64(', 'int\nSys_print64(', 'int\nSys_fprint64(', 'int\nSys_setframes64(', 'int\nSys_clearframes64(', 'int\nSys_bindsite64(', 'int\nSys_callsite64(', 'int\nSys_bindframe64(', 'int\nSys_sprintframe64(', 'int\nSys_aprintframe64(', 'int\nSys_printframe64(', 'int\nSys_fprintframe64('):
                        source += '\n'+function(runtime, signature)+'\n'
                    extra = [str(ROOT/('lib9/'+name+'.c')) for name in ('rune','sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+['-pthread']
                unit.write_text(source)
                binary = work/(label+'-'+port)
                subprocess.run(command+[str(unit)]+objects+extra+['-Wl,--wrap=free', '-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
                subprocess.run(runner+[str(binary)], env=env, check=True)
                for control in ('frame', 'types'):
                    result = subprocess.run(runner+[str(binary), control], env=env, capture_output=True, text=True)
                    if result.returncode != -signal.SIGABRT or 'GC root predicate failed' not in result.stderr:
                        raise RuntimeError('Argument ownership control did not reach its predicate: '+control+'\n'+result.stdout+result.stderr)
                if args.formatter:
                    for name, header, boundary, replacement in (('kind', 'native64-format.h', 'if(!valid)', 'if(0)'),
                            ('output', 'native64-format.h', 'native64formattedclear(&build.output);', '(void)0;'),
                            ('sys-result', 'native64-sys-format.h', 'if(!native64sysformatreference(nil, old, call->array ? &Tarray : &Tstring))', 'if(0)'),
                            ('sys-element', 'native64-sys-format.h', 'if(call->array && old != H && ((Array*)old)->t != &Tbyte)', 'if(0)'),
                            ('write-acquire', 'native64-sys-write.h', 'native64syswriteacquire(&call);', '(void)0;'),
                            ('write-fd-map', 'native64-sys-write.h', 'uchar fprintmap[] = F_Sys_fprint_map;', 'uchar fprintmap[] = {0, 0x40};'),
                            ('vm-return', 'native64-sys-frame.h', 'return (uintptr)address == result ? (void*)result : nil;', 'return (void*)result;'),
                            ('vm-map', 'native64-sys-frame.h', 'if(type->map[i] != expected) return 0;', 'if(0) return 0;'),
                            ('bind-width', 'native64-sys-frame.h', 'memmove((uchar*)call.request.frame+32, &result, sizeof(result));', 'memmove((uchar*)call.request.frame+32, &result, sizeof(WORD));'),
                            ('bind-pending', 'native64-sys-frame.h', ': active.mr != nil)', ': 0)'),
                            ('sites-copy', 'native64-sys-sites.h', 'memmove(site->slots, site->spec.slots, site->spec.count*sizeof(*site->slots));', 'memset(site->slots, 0, site->spec.count*sizeof(*site->slots));'),
                            ('sites-owner', 'native64-sys-sites.h', 'if(((Modlink*)active.mr)->m != module) return 0;', '(void)0;'),
                            ('sites-retire', 'native64-sys-sites.h', 'native64syssitesdiscard(table);', 'free(table);'),
                            ('store-canonical', '@unit', 'case ICVTWL: cvtwl(); break; // Canonical eight-byte integer argument.', 'case ICVTWL: movw(); break; // Deliberately omit sign extension.'),
                            ('store-reference', '@unit', 'case IMOVP: movp(); break; // Managed retain/mark before result publication.', 'case IMOVP: memmove(R.d, R.s, sizeof(void*)); break; // Deliberately omit ownership.'),
                            ('source-relocation', '@unit', 'sourceoffset = store->source_offset; // Compiler-selected source relocation.', 'sourceoffset = store->destination; // Deliberately confuse source and destination banks.'),
                            ('source-map', '@unit', 'type = dtype(freeheap, total, fullmap, (total+63)/64);', '{ uchar zeros[SlotBytes/64+1] = {0}; type = dtype(freeheap, total, zeros, (total+63)/64); }'),
                            ('source-once', '@unit', 'if(filled[store->cell]) continue;', 'if(0) continue;'),
                            ('source-cache', '@unit', 'movp(); // Refresh the bank root at every original source read.', 'if(!refreshed[store->cell]) movp(); // Deliberately cache the first reference read.'),
                            ('source-origin', '@unit', 'origin = (uchar*)input.value+bases[cell->space]+cell->offset;', 'origin = (uchar*)input.value+sourceoffset; // Deliberately read the unfilled bank instead of FP/MP.'),
                            ('initializer-value', '@unit', 'pattern = c2string((char*)seed->initial, seed->length);', 'pattern = c2string("%d|%s", 5);'),
                            ('initializer-bits', '@unit', 'bits = ((uvlong)cell->high<<32)|cell->low; // Compiler-owned canonical initializer bits.', 'bits = cell->low; // Deliberately drop the high initializer word.')):
                        adapter = source if header == '@unit' else (ROOT/('include/'+header)).read_text()
                        if adapter.count(boundary) != 1:
                            raise RuntimeError('Typed format control boundary changed: '+name)
                        control_unit = work/(port+'-'+name+'-unchecked.c')
                        adapter = adapter.replace(boundary, replacement)
                        if name == 'write-fd-map':
                            if adapter.count('fprintmap[1] != 0xc0') != 1:
                                raise RuntimeError('FD map guard changed')
                            adapter = adapter.replace('fprintmap[1] != 0xc0', 'fprintmap[1] != 0x40')
                        control_source = adapter if header == '@unit' else source.replace('#include "'+header+'"', adapter, 1)
                        if name == 'write-fd-map':
                            schema = (work/'sys-native64.h').read_text()
                            boundary = '#define F_Sys_fprint_map {0x00,0xc0}'
                            if schema.count(boundary) != 1:
                                raise RuntimeError('Generated FD map control boundary changed')
                            schema = schema.replace(boundary, '#define F_Sys_fprint_map {0x00,0x40}')
                            control_source = control_source.replace('#include "sys-native64.h"', schema, 1)
                        control_unit.write_text(control_source)
                        control_binary = work/(label+'-'+port+'-'+name+'-unchecked')
                        subprocess.run(command+[str(control_unit)]+objects+extra+['-Wl,--wrap=free', '-Wl,--gc-sections', '-lm', '-o', str(control_binary)], env=env, check=True)
                        result = subprocess.run(runner+[str(control_binary)], env=env, capture_output=True, text=True)
                        if result.returncode != -signal.SIGABRT or 'GC root predicate failed' not in result.stderr:
                            raise RuntimeError('Typed format control did not reach its predicate: '+name+'\n'+result.stdout+result.stderr)
                        print('PASS typed formatter removal control:', name, port, label, flush=True)
                print('PASS managed argument contexts:', port, label, flush=True)


if __name__ == '__main__':
    main()
