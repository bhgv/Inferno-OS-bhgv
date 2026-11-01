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
    print('PASS clean classic-mk native64 boot/lease gates; logs: '+str(logs))
    print('Scope: self-checking scalar programs and trusted C fixtures; file CALL/MCALL and full shell startup remain closed.')


if __name__ == '__main__':
    main()
