#!/usr/bin/env python3
"""Run actual vmachine error selection with bounded single-worker OS boundaries."""
import os
from pathlib import Path
import platform
import signal
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SUPPORT = {'__file__': str(ROOT/'libinterp/tests/dis64-scheduler.py'), '__name__': 'scheduler_support'}
exec(compile((ROOT/'libinterp/tests/dis64-scheduler.py').read_bytes(), SUPPORT['__file__'], 'exec'), SUPPORT)
function = SUPPORT['function']


# Reuse scheduler ownership instrumentation while borrowing whole production functions.
def buildunit(filemode=False):
    if filemode:
        files = {'__file__': str(ROOT/'libinterp/tests/dis64-fileloader.py'), '__name__': 'fileloader_support'}
        exec(compile((ROOT/'libinterp/tests/dis64-fileloader.py').read_bytes(), files['__file__'], 'exec'), files)
        unit = files['buildunit']()
    else:
        unit = SUPPORT['buildunit']()
    unit = unit[:unit.index('/* Execute a real root process, retain its result owner')]
    unit = unit.replace('void\ncleanexit(', 'void\nschedulercleanexit(', 1)
    scheduler = (ROOT/'emu/port/dis.c').read_text()
    exception = (ROOT/'emu/port/exception.c').read_text()
    parts = [function(scheduler, signature) for signature in (
        'int\ntready(', 'void\ndelrunq(', 'static Prog*\ngrpleader(', 'int\nexprog(',
        'static void\npropex(', 'void\nstartup(', 'void\nvmachine(')]
    parts += [function((ROOT/'emu/port/devprog.c').read_text(), 'WORD\nmodstatus(')]
    parts += ['static String *exstr;\n#define NOPC ((ulong)-1) // Native absent-handler coordinate.']
    parts += [function(exception, signature) for signature in (
        'static int\nematch(', 'static void\nsetstr(', 'static String*\nnewestring(',
        'static Type*\nframetype(', 'static void\nfreeframe(', 'static int\nexceptionframe(',
        'static ulong\nexceptionpc(', 'static int\nexceptiontarget(', 'int\nhandler(')]
    # Count genuine calls/results without replacing the selected handler body.
    parts[-1] = parts[-1].replace('int\nhandler(', 'int\nactualhandler(', 1)
    fixture = (ROOT/'libinterp/tests/dis64-vmachine.c').read_text()
    if filemode:
        fixture = fixture.replace(function(fixture, 'String*\nc2string('), '')
        fixture = fixture.replace('prepare64pilot(', 'fileprepare(')
    return unit+fixture.replace('VMACHINE_IMPLEMENTATION', '\n\n'.join(parts))


# Keep the exact same four immutable input profiles as the scheduler/fileloader gates.
def main():
    if platform.system() != 'Linux' or platform.machine().lower() not in ('amd64', 'x86_64'):
        print('SKIP Dis64 vmachine RX: requires Linux/AMD64')
        return
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    with tempfile.TemporaryDirectory(prefix='dis64-vmachine-') as temporary:
        work = Path(temporary)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
        command += ['-DDIS_WORD32=1']
        command += ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        binaries = []
        for label, filemode in (('direct', False), ('file', True)):
            source = work/(label+'.c')
            source.write_text(buildunit(filemode))
            binary = work/label
            flags = ['-DDIS64_PILOT_LOADER=1'] if filemode else []
            subprocess.run(command+flags+[str(source), str(ROOT/'libinterp/dec.c'), '-o', str(binary), '-lm', '-Wl,--wrap=free'], check=True)
            binaries.append((label, binary))
        for name in ('pilot42', 'pilot-init42', 'divide-zero', 'shift-invalid'):
            image = work/(name+'.dis64')
            if name in ('divide-zero', 'shift-invalid'):
                operation = '43100000' if name == 'divide-zero' else '4e102000'
                image.write_bytes(SUPPORT['SUPPORT']['image']([bytes.fromhex(operation), bytes.fromhex('0f1b')]))
            else:
                image.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64'/(name+'.hex')).read_text()))
            for label, binary in binaries:
                print(label, name, flush=True)
                subprocess.run([str(binary), str(image), '1' if name == 'divide-zero' else '2' if name == 'shift-invalid' else '0'], check=True)

        # Each control must compile and then fail a live runtime predicate.
        controlimage = work/'divide-zero.dis64'
        controls = (
            ('no-propagation', 'static void\npropex(',
             'static void\npropex(Prog *p, char *estr) { (void)p; (void)estr; }'),
            ('no-selection', 'int\nactualhandler(',
             'int\nactualhandler(char *estr) { (void)estr; return 0; }'),
        )
        for label, signature, replacement in controls:
            unit = buildunit()
            source = work/(label+'.c')
            source.write_text(unit.replace(function(unit, signature), replacement, 1))
            binary = work/label
            subprocess.run(command+[str(source), str(ROOT/'libinterp/dec.c'), '-o', str(binary), '-lm', '-Wl,--wrap=free'], check=True)
            run = subprocess.run([str(binary), str(controlimage), '1'], capture_output=True, text=True)
            if run.returncode != -signal.SIGABRT or 'Dis64 runtime check failed' not in run.stderr:
                raise RuntimeError(label+' did not reproduce a runtime predicate failure: '+run.stderr)
            print('PASS negative control: '+label+' compiled and failed an active runtime predicate', flush=True)


if __name__ == '__main__':
    main()
