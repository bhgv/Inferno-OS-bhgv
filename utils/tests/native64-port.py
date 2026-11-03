#!/usr/bin/env python3
"""Build a clean Linux native64 emulator with classic mk and run the owning boot/lease gates."""
from pathlib import Path
import argparse
import os
import platform
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


# Keep diagnostics on disk and expose the relevant tail when a compiler or gate fails.
def run(command, cwd, env, log):
    print('RUN', log.name, flush=True)
    with log.open('w') as stream:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print('\n'.join(log.read_text(errors='replace').splitlines()[-40:]), file=sys.stderr)
        raise RuntimeError('Failed '+log.name+'; complete log: '+str(log))
    print('PASS', log.name, flush=True)


# Copy tracked inputs, including current edits, without importing stale objects or user scratch files.
def sources(destination):
    names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).split(b'\0')
    for name in names:
        if not name:
            continue
        relative = Path(os.fsdecode(name))
        source = ROOT/relative
        target = destination/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target, follow_symlinks=False)


# Exercise the coherent experimental profile; successful pilot boot is not full Limbo shell admission.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root', type=Path, required=True, help='empty directory outside the source checkout')
    parser.add_argument('--host-tools', type=Path, required=True, help='native mk, limbo, iyacc and decgen from makemk.sh')
    parser.add_argument('--arch', choices=('amd64', 'arm64', 'riscv64'), default='arm64')
    parser.add_argument('--cross', default='', help='literal target tool prefix consumed by classic mk')
    parser.add_argument('--test-cc', default='', help='owning C gate compiler command; defaults to CROSS gcc (use dynamic libc for GNU --wrap=free fixtures)')
    parser.add_argument('--runner', default='', help='target execution command, e.g. qemu-aarch64 -L /sysroot')
    parser.add_argument('--test-cflags', default='-O3 -g -DNDEBUG -fno-strict-aliasing', help='additional owning C gate flags; add UBSan where available')
    args = parser.parse_args()
    build = args.build_root.resolve()
    tools = args.host_tools.resolve()
    if platform.system() != 'Linux':
        parser.error('This reproduction driver currently requires a Linux build host')
    if build == ROOT or ROOT in build.parents or build in ROOT.parents:
        parser.error('Build root must be outside, and not contain, the source checkout')
    if build.exists() and any(build.iterdir()):
        parser.error('Build root must be empty; old ABI objects must not be reused')
    for name in ('mk', 'limbo', 'iyacc', 'decgen'):
        if not os.access(tools/name, os.X_OK):
            parser.error('Missing native host tool: '+str(tools/name))
    # Reject mk metacharacters before copying inputs or changing any destination files.
    sys.path.insert(0, str(ROOT/'utils'))
    import configure
    try:
        configure.literal(str(build))
        if args.cross:
            configure.literal(args.cross)
    except argparse.ArgumentTypeError as error:
        parser.error(str(error))
    build.mkdir(parents=True, exist_ok=True)
    sources(build)
    logs = build/'port-logs'; logs.mkdir()
    temporary = build/'port-tmp'; temporary.mkdir()
    env = dict(os.environ, PATH=str(tools)+os.pathsep+os.environ.get('PATH', ''), TMPDIR=str(temporary),
               NATIVE64_TMPDIR=str(temporary), NATIVE64_ARCH=args.arch, NATIVE64_RUNNER=args.runner,
               NATIVE64_CFLAGS=args.test_cflags, CC=args.test_cc or shlex.join([args.cross+'gcc']))
    configure_command = [sys.executable, str(build/'utils/configure.py'), '--host', 'linux', '--arch', args.arch,
                         '--root', str(build), '--console', '--native64-scalar-boot', '--native64-math-boot',
                         '--native64-sys-io-boot', '--native64-caller-guard', '--cross', args.cross,
                         '--output', str(build/'mkconfig')]
    run(configure_command, build, env, logs/'configure.log')
    binary = build/'Linux'/args.arch/'bin'; binary.mkdir(parents=True, exist_ok=True)
    (binary.parent/'lib').mkdir(exist_ok=True)
    for name in ('mk', 'limbo', 'iyacc', 'decgen'):
        target = binary/name
        if target.exists() or target.is_symlink():
            target.unlink()
        target.symlink_to(tools/name)
    run([str(tools/'mk'), 'CONF=emu-g', 'CONFLIST=emu-g'], build/'emu/Linux', env, logs/'mk.log')
    tests = build/'utils/tests'
    builtinenv = dict(env, NATIVE64_LIMBO=str(tools/'limbo'))
    run([sys.executable, str(tests/'native64-builtins.py'), '--sets', '--port-aliases'],
        build, builtinenv, logs/'builtin-port-aliases.log')
    run([sys.executable, str(tests/'native64-scalar-boot.py'), '--root', str(build),
         '--emu', str(build/'emu/Linux/o.emu-g'), '--runner', args.runner, '--math', '--sys-io'], build, env, logs/'boot.log')
    run([sys.executable, str(tests/'native64-limbo-boot.py'), '--limbo', str(tools/'limbo'),
         '--root', str(build), '--emu', str(build/'emu/Linux/o.emu-g'), '--runner', args.runner],
        build, env, logs/'limbo-boot.log')
    run([sys.executable, str(tests/'dis64-location-read.py')], build, env, logs/'dis64-location-read.log')
    run([sys.executable, str(tests/'dis64-locations.py'), '--limbo', str(tools/'limbo'),
         '--root', str(build), '--emu', str(build/'emu/Linux/o.emu-g'), '--runner', args.runner],
        build, env, logs/'dis64-locations.log')
    run([sys.executable, str(tests/'dis64-cases.py'), '--limbo', str(tools/'limbo'),
         '--root', str(build), '--emu', str(build/'emu/Linux/o.emu-g'), '--runner', args.runner],
        build, env, logs/'dis64-cases.log')
    run([sys.executable, str(tests/'dis64-setup-targets.py')], build, env, logs/'dis64-setup-targets.log')
    run([sys.executable, str(tests/'dis64-tuple-layout.py')], build, env, logs/'dis64-tuple-layout.log')
    run([sys.executable, str(tests/'dis64-copy-layout.py')], build, env, logs/'dis64-copy-layout.log')
    catalog = [sys.executable, str(tests/'dis64-local-catalog.py')]
    run(catalog, build, env, logs/'local-catalog.log')
    flow = [sys.executable, str(tests/'dis64-local-flow.py')]
    run(flow, build, env, logs/'local-flow.log')
    for control in ('branch', 'fallthrough', 'calls', 'cycle', 'depth'):
        run(flow+['--control', control], build, env, logs/('local-flow-'+control+'.log'))
    state = [sys.executable, str(tests/'dis64-local-state.py')]
    run(state, build, env, logs/'local-state.log')
    for control in ('arguments', 'binding', 'reuse', 'kind', 'boolean', 'return', 'retire',
                    'join', 'canonical', 'preparation', 'address', 'identity', 'result'):
        run(state+['--control', control], build, env, logs/('local-state-'+control+'.log'))
    cfg = [sys.executable, str(tests/'dis64-local-cfg.py')]
    run(cfg, build, env, logs/'local-cfg.log')
    for control in ('entry', 'unreachable', 'setup', 'site', 'join', 'capacity', 'limit', 'reset'):
        run(cfg+['--control', control], build, env, logs/('local-cfg-'+control+'.log'))
    operands = [sys.executable, str(tests/'dis64-local-operands.py')]
    run(operands, build, env, logs/'local-operands.log')
    for control in ('narrow', 'width', 'tail', 'overlap', 'kind', 'bool', 'implicit', 'shift',
                    'unreachable', 'atomic', 'capacity', 'publication'):
        run(operands+['--control', control], build, env, logs/('local-operands-'+control+'.log'))
    initialized = [sys.executable, str(tests/'dis64-local-initialized.py')]
    run(initialized, build, env, logs/'local-initialized.log')
    for control in ('intersection', 'source', 'middle', 'self_read', 'seed', 'parameter_kind',
                    'parameter_span', 'requeue', 'capacity', 'budget', 'reset', 'unreachable', 'publication'):
        run(initialized+['--control', control], build, env, logs/('local-initialized-'+control+'.log'))
    bound = [sys.executable, str(tests/'dis64-bound-numeric.py')]
    run(bound, build, env, logs/'bound-numeric.log')
    for control in ('symbol', 'entry', 'frame', 'signature', 'framebytes', 'interval', 'rebase',
                    'root_return', 'callee_exit', 'fence', 'output_capacity', 'work_capacity', 'budget', 'atomic'):
        run(bound+['--control', control], build, env, logs/('bound-numeric-'+control+'.log'))
    returns = [sys.executable, str(tests/'dis64-bound-return.py')]
    run(returns, build, env, logs/'bound-return.log')
    for control in ('symbol', 'entry', 'frame', 'signature', 'framebytes', 'interval', 'rebase',
                    'root_return', 'callee_exit', 'fence', 'output_capacity', 'work_capacity', 'budget',
                    'atomic', 'base', 'secondary', 'store_kind', 'bool', 'void', 'source', 'cut',
                    'branch', 'terminal', 'exists', 'reachable', 'return_budget'):
        run(returns+['--control', control], build, env, logs/('bound-return-'+control+'.log'))
    calls = [sys.executable, str(tests/'dis64-bound-call.py')]
    run(calls, build, env, logs/'bound-call.log')
    for control in ('symbol', 'entry', 'frame', 'signature', 'callee_exit', 'work_capacity',
                    'output_capacity', 'budget', 'steps', 'atomic', 'pointer_temp', 'frame_type',
                    'call_pointer', 'bind_owner', 'bind_source', 'bind_destination', 'bind_width',
                    'argument_offset', 'argument_source', 'argument_complete', 'pending', 'linear',
                    'ready', 'result_kind', 'result_write', 'leaf_return', 'leaf_address',
                    'conversion_source', 'conversion_destination'):
        run(calls+['--control', control], build, env, logs/('bound-call-'+control+'.log'))
    for control in ('entries', 'tail'):
        run(catalog+['--control', control], build, env, logs/('local-catalog-'+control+'.log'))
    run([sys.executable, str(tests/'dis64-inline.py'), '--limbo', str(tools/'limbo'),
         '--root', str(build), '--emu', str(build/'emu/Linux/o.emu-g'), '--runner', args.runner],
        build, env, logs/'dis64-inline.log')
    utfenv = dict(env, CFLAGS=env['NATIVE64_CFLAGS'])
    run([sys.executable, str(build/'libinterp/tests/sys-utf.py')], build, utfenv, logs/'sys-utf.log')
    run([sys.executable, str(build/'libinterp/tests/sys-io.py')], build, utfenv, logs/'sys-io.log')
    run([sys.executable, str(tests/'native64-guard-link.py'), '--root', str(build), '--log', str(logs/'mk.log'),
         '--ar', args.cross+'ar'], build, env, logs/'guard-link.log')
    run([sys.executable, str(tests/'native64-build-deps.py'), '--root', str(build), '--mk', str(tools/'mk')],
        build, env, logs/'build-deps.log')
    math = [sys.executable, str(tests/'native64-fixed-math.py'), '--caller-guard', '--limbo', str(tools/'limbo'),
            '--libdir', str(binary.parent/'lib')]
    for port in ('emu', 'os'):
        run(math+['--caller-port', port], build, env, logs/('math-'+port+'.log'))
    run(math+['--inject-callee'], build, env, logs/'math-unwind.log')
    gc = [sys.executable, str(tests/'native64-frame-cell.py'), '--gateway', '--lease', '--limbo', str(tools/'limbo')]
    run(gc, build, env, logs/'gc-lease.log')
    run(gc+['--control', 'map'], build, env, logs/'gc-map-control.log')
    run(gc+['--live'], build, env, logs/'gc-live.log')
    run(gc+['--live', '--control', 'map'], build, env, logs/'gc-live-map-control.log')
    calls = [sys.executable, str(tests/'native64-local-calls.py')]
    run(calls, build, env, logs/'local-calls.log')
    run(calls+['--control'], build, env, logs/'local-calls-control.log')
    results = [sys.executable, str(tests/'native64-result-calls.py')]
    binding = [sys.executable, str(tests/'native64-disc-binding.py')]
    run(binding, build, env, logs/'disc-binding.log')
    for control in ('mode', 'jit', 'image'):
        run(binding+['--control', control], build, env, logs/('disc-binding-'+control+'.log'))
    loader_mode = [sys.executable, str(tests/'native64-loader-mode.py')]
    run(loader_mode, build, env, logs/'loader-mode.log')
    run(loader_mode+['--control'], build, env, logs/'loader-mode-control.log')
    run([sys.executable, str(build/'libinterp/tests/loader-snapshots.py')],
        build, env, logs/'loader-snapshots.log')
    loader_branch = [sys.executable, str(tests/'native64-loader-branch.py')]
    run(loader_branch, build, env, logs/'loader-branch.log')
    for control in ('width', 'bounds'):
        run(loader_branch+['--control', control], build, env, logs/('loader-branch-'+control+'.log'))
    graphics_init = [sys.executable, str(tests/'native64-graphics-init.py')]
    for module in ('prefab', 'freetype'):
        run(graphics_init+['--module', module, '--limbo', str(tools/'limbo')], build, env, logs/('graphics-init-'+module+'.log'))
        controls = ('retirement', 'publication', 'registration')+(('face-map', 'glyph-map') if module == 'freetype' else ())
        for control in controls:
            run(graphics_init+['--module', module, '--control', control], build, env, logs/('graphics-init-'+module+'-'+control+'.log'))
    run([sys.executable, str(build/'libinterp/tests/gui-type-maps.py')], build, env, logs/'gui-type-maps.log')
    loader_init = [sys.executable, str(tests/'native64-loader-init.py')]
    run(loader_init, build, env, logs/'loader-init.log')
    for control in ('retirement', 'publication', 'registration'):
        run(loader_init+['--control', control], build, env, logs/('loader-init-'+control+'.log'))
    draw_init = [sys.executable, str(tests/'native64-draw-init.py')]
    run(draw_init, build, env, logs/'draw-init.log')
    for control in ('retirement', 'publication', 'registration'):
        run(draw_init+['--control', control], build, env, logs/('draw-init-'+control+'.log'))
    run(results, build, env, logs/'result-calls.log')
    for control in ('width', 'priority', 'ordinary', 'guard_mode', 'direct', 'entry', 'cache'):
        run(results+['--control', control], build, env, logs/('result-calls-'+control+'.log'))
    run(results+['--instrument'], build, env, logs/'result-instrument.log')
    run(results+['--instrument', '--instrument-source'], build, env, logs/'result-instrument-source.log')
    run(results+['--instrument', '--control', 'priority'], build, env, logs/'result-instrument-priority.log')
    handlers = [sys.executable, str(tests/'native64-local-handlers.py')]
    run(handlers, build, env, logs/'local-handlers.log')
    for control in ('pending', 'registers'):
        run(handlers+['--control', control], build, env, logs/('local-handlers-'+control+'.log'))
    fresh = [sys.executable, str(tests/'native64-fresh-handlers.py')]
    run(fresh, build, env, logs/'fresh-handlers.log')
    for control in ('publication', 'map'):
        run(fresh+['--control', control], build, env, logs/('fresh-handlers-'+control+'.log'))
    vm = [sys.executable, str(tests/'native64-live-vm.py')]
    run(vm, build, env, logs/'live-vm.log')
    for control in ('enter', 'unwind'):
        run(vm+['--control', control], build, env, logs/('live-vm-'+control+'.log'))
    field = [sys.executable, str(tests/'native64-field.py'), '--limbo', str(tools/'limbo')]
    run(field, build, env, logs/'field-witness.log')
    for control in ('kind', 'owner', 'generation', 'field'):
        run(field+['--control', control], build, env, logs/('field-witness-'+control+'.log'))
    if args.arch in ('arm64', 'riscv64'):
        run([sys.executable, str(tests/'native64-runtime.py'), '--arch', args.arch, '--system', 'Linux', '--cc', env['CC'],
             '--runner', args.runner], build, env, logs/(args.arch+'-runtime.log'))
    if args.arch == 'riscv64':
        run([sys.executable, str(build/'libinterp/tests/riscv-dispatch.py'), '--cc', env['CC'],
             '--runner', args.runner], build, env, logs/'riscv-complete-tails.log')
    print('PASS clean classic-mk native64 boot/lease gates; logs: '+str(logs))
    print('Scope: self-checking scalar programs and trusted C fixtures; file CALL/MCALL and full shell startup remain closed.')


if __name__ == '__main__':
    main()
