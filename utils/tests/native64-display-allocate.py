#!/usr/bin/env python3
"""Check actual Frame64 Display allocation and staged rollback without registering managed Draw."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Reuse the actual Heap/Type/Draw ownership closure, replacing only native service boundaries.
def unit(control=None):
    helpers = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    fn = helpers['function']
    source = runpy.run_path(str(ROOT/'utils/tests/native64-draw-owners.py'))['unit']()
    source = source.replace(fn(source, 'int\nmain('), '', 1)
    declarations = '''static int allocationfail, allocationstep; // Managed/DRef service fault schedule.
static int inputready; // Final native capture of a possibly aliased device string.
static void *inputwatch; // Actual managed String owner whose premature retirement must stop.
static void displaycheck(int, char*); // Exact semantic stages, active under NDEBUG.
static void displayobserve(void); // Observe packed result publication before native services.
static void allocationrootobserve(void); // Observe actual pinned parent reachability before nested heap allocation.
'''
    source = source.replace('static int live, allocations', 'static int allocationfail, allocationstep; // Deterministic managed/DRef allocation schedule.\nstatic void allocationrootobserve(void); // Bounded external-root observations.\nstatic int live, allocations', 1)
    declarations = declarations.replace('static int allocationfail, allocationstep; // Managed/DRef service fault schedule.\n', '')
    source = source.replace('char exType[]=', declarations+'\nchar exType[]=', 1)
    source = source.replace('actualdestroy(value);', 'if(value == inputwatch) displaycheck(inputready, "input lifetime");\n\tactualdestroy(value);', 1)
    pool = fn(source, 'void*\npoolalloc(')
    source = source.replace(pool, pool.replace('block = trackedallocate(', 'if(allocationstep >= 3) allocationrootobserve();\n\tif(++allocationstep == allocationfail) return nil;\n\tblock = trackedallocate(', 1), 1)
    # Unrelated fixture allocations never enter this deterministic managed/DRef schedule.
    close = fn(source, 'void\nclosedisplay(')
    source = source.replace(close, 'void allocationclosedisplay(Display*); // Native defaults are separately owned during rollback.\n'+close.replace('nativeretire(d,4);', 'allocationclosedisplay(d);'), 1)
    close = fn(source, 'void\nsubfont_close(')
    source = source.replace(close, 'void allocationsubfontclose(Subfont*); // Bounded optional default-font service.\n'+close.replace('USED(font); require(0);', 'allocationsubfontclose(font);'), 1)
    build = fn(source, 'Font*\nbuildfont(')
    source = source.replace(build, 'static int fontpolicy; // Optional font/cache service outcomes.\n'+build.replace('miss=fontmiss;', 'if(fontpolicy == 2) return nil;\n\tmiss=fontmiss;'), 1)
    cache = fn(source, 'Cache*\ncacheinstall(')
    source = source.replace(cache, 'static Cache defaultcache; // Bounded optional cache-service identity.\n'+cache.replace('return nil;', 'if(fontpolicy != 3) return nil;\n\tdefaultcache.display=d; defaultcache.u.ptr=value; defaultcache.ref=0;\n\treturn &defaultcache;', 1), 1)
    close = fn(source, 'void\nfont_close(')
    source = source.replace(close, close.replace('nativeretire(font,3);', 'if(defaultcache.u.f == font){ displaycheck(defaultcache.ref == 1, "default cache reference"); defaultcache.ref=0; defaultcache.u.f=nil; }\n\tnativeretire(font,3);', 1), 1)
    # The unchanged cached-display helper is compiled against real public context records.
    services = (ROOT/'utils/tests/native64-display-allocate.c').read_text()
    services = services.replace('static Cache defaultcache; // Bounded optional cache-service record, not its lookup algorithm.\n', '')
    services = services.replace('static int allocationcases, initcalls, initfailure, lockfailure, fontpolicy, fontcalls;', 'static int allocationcases, initcalls, initfailure, lockfailure, fontcalls;')
    services = services.replace('static int rollbackexpected, inputready;', 'static int rollbackexpected;')
    services = services.replace('static void *inputwatch, *resultwatch, *resultold;', 'static void *resultwatch, *resultold;')
    services = services.replace('ALLOCATION_IMPLEMENTATION', 'void *cacheqlock; // Bounded process-wide lock service.\nALLOCATION_IMPLEMENTATION')
    draw = (ROOT/'libinterp/draw.c').read_text()
    implementation = fn(draw, 'static int\ndrawcachedisplay(')+'\n\n'+draw[draw.index('typedef struct DrawDisplayImages'):draw.index('/* Capture inputs and publish')]+'\n\n'+fn(draw, 'void\nDisplay_allocate(')
    implementation = implementation.replace('malloc(sizeof(*reference))', 'allocationlease()').replace('free(reference);', 'nativeretire(reference,5);')
    implementation = implementation.replace('F_Display_allocate', 'F_Draw_Display_allocate').replace('\nDisplay_allocate(', '\nDraw_Display_allocate(')
    if control == 'input':
        implementation = implementation.replace('device = f->dev;', 'device = f->dev; destroy(old); old = H;', 1)
    elif control == 'publication':
        implementation = implementation.replace('result = drawallocatedisplay(', 'memmove(slot,&result,sizeof(result)); result = drawallocatedisplay(', 1)
    elif control == 'image':
        implementation = implementation.replace('((DImage*)build.images[i])->image = nil;', '/* omitted native ownership restoration */', 1)
    elif control == 'display':
        implementation = implementation.replace('owner->display = nil;', '/* omitted native ownership restoration */', 1)
    elif control == 'root':
        implementation = implementation.replace('ptradd(D2H(build->owner));', '/* omitted parent root */', 1)
    elif control == 'edges':
        implementation = implementation.replace('*build->slots[i] = build->images[i];', '/* omitted mapped parent edge */', 1)
    elif control == 'unlock':
        implementation = implementation.replace('kgerrstr(reason, sizeof(reason));\n\tlibqunlock(display->qlock);', 'kgerrstr(reason, sizeof(reason));', 1)
    gc = (ROOT/'libinterp/gc.c').read_text()
    roots = gc[gc.index('typedef struct Ptrhash'):gc.index('\n\tint\tnprop;')]
    buckets = next(line for line in gc.splitlines() if 'PTRHASH =' in line).split('=',1)[1].split(',',1)[0].strip()
    roots += '\n#define PTRHASH '+buckets+' // Actual native external-root hash geometry.\nstatic Ptrhash *ptrtab[PTRHASH], *ptrfree; // Actual collector registry and reusable nodes.\n'
    roots += next(line for line in gc.splitlines() if line.startswith('#define\tHASHPTR'))+'\n'
    roots += '''/* Inject only external-root metadata allocation, preserving ptradd's real error path. */
static void*
allocationrootmemory(void)
{
    if(++allocationstep == allocationfail) return nil;
    return trackedallocate(sizeof(Ptrhash),0);
}
#define malloc(n) allocationrootmemory() // Actual ptradd metadata service boundary.
'''+fn(gc,'void\nptradd(')+'\n'+fn(gc,'void\nptrdel(')+'\n#undef malloc\n'
    services = services.replace('ROOT_IMPLEMENTATION', roots)
    services = services.replace('ALLOCATION_IMPLEMENTATION', '''/* Refuse the lease before opening channels, without mixing service-storage allocations into the schedule. */
static void*
allocationlease(void)
{
    if(++allocationstep == allocationfail){
        kwerrstr("memory");
        return nil;
    }
    return nativeallocate(sizeof(DRef),5);
}

'''+implementation)
    services = services.replace('allocationmain(', 'main(', 1)
    return helpers['portfixture'](source+'\n\n'+services, '#include "vmfetch.h"')


# A failure must name its semantic stage; compile, sanitizer, signal and timeout never satisfy controls.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--control', choices=('input', 'publication', 'image', 'display', 'unlock', 'root', 'edges'))
    args = parser.parse_args()
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    flags += ['-ffunction-sections', '-fdata-sections', '-DDIS_WORD32=1']
    if arch == 'arm64':
        flags += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
    elif arch == 'riscv64':
        flags += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='native64-display-allocate-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', '-M', 'Draw', '-I'+str(ROOT/'module'), str(ROOT/'module/draw-native64.m')], env=env))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        path = work/'test.c'
        guard = {'amd64': '!defined(__x86_64__)', 'arm64': '!defined(__aarch64__)', 'riscv64': '!defined(__riscv) || __riscv_xlen != 64'}[arch]
        path.write_text('#if '+guard+'\n#error native target compiler mismatch\n#endif\n'+unit(args.control))
        for model in (('control',) if args.control else ('WORD32', 'WORD32-file')):
            binary = work/model
            extra = ['-DDIS64_PILOT_LOADER=1'] if model == 'WORD32-file' else []
            subprocess.run(compiler+flags+extra+includes+[str(path), '-Wl,--gc-sections', '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=90)
            if args.control:
                stage = {'input': 'input lifetime', 'publication': 'early publication', 'image': 'native rollback ownership', 'display': 'native rollback ownership', 'unlock': 'close unlocked', 'root': 'construction root', 'edges': 'construction edges'}[args.control]
                if result.returncode != 10 or result.stderr.strip() != 'Display allocation mismatch: '+stage:
                    raise RuntimeError('control missed semantic predicate: '+result.stdout+result.stderr)
                print('PASS Display.allocate semantic control:', args.control, arch, flush=True)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(model+': '+result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
