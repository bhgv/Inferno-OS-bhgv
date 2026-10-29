#!/usr/bin/env python3
"""Link and run the native Limbo compiler on real sources, then validate its Dis output."""
import hashlib
import os
from pathlib import Path
import platform
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning compiler, libraries and bootstrap.


# Return complete diagnostics while distinguishing expected compiler failures.
def run(command, cwd, environment, success=True):
    result = subprocess.run(command, cwd=cwd, env=environment, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != success:
        raise RuntimeError(shlex.join(map(str, command))+'\n'+result.stdout)
    return result.stdout


# Check exact bytes rather than timestamps when preserving already installed tools.
def fingerprints(paths):
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            if path.is_file() else None for path in paths}


# Exercise real grammar, compiler/link dependencies, CLI errors and generated modules.
def main():
    machine = platform.machine()
    if platform.system() != 'Linux' or machine not in ('x86_64', 'aarch64'):
        raise RuntimeError('native Limbo integration requires Linux x64 or arm64')
    arch, elf = ('amd64', 62) if machine == 'x86_64' else ('arm64', 183)
    protected = [ROOT/'mkconfig', ROOT/'limbo/y.tab.c', ROOT/'limbo/y.tab.h',
                 ROOT/'libsec/port/md5.c', ROOT/'libsec/port/md5block.c']
    before = fingerprints(protected)
    environment = dict(os.environ, MKHOSTOS='Linux', MKHOSTARCH=arch,
                       INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_LIMBO='1')
    environment['CFLAGS'] = os.environ.get('NATIVE_LIMBO_CFLAGS',
        '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all '
        '-Werror=implicit-function-declaration -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast')
    with tempfile.TemporaryDirectory(prefix='native Limbo integration ') as temporary:
        work = Path(temporary)
        tools = work/'native tools'
        environment['MKBOOTSTRAP_BINDIR'] = str(tools)
        environment['ROOT'] = str(work/'unrelated source root')
        run(['sh', str(ROOT/'makemk.sh')], work, environment)
        binaries = [tools/name for name in ('mk', 'iyacc', 'decgen', 'limbo')]
        for binary in binaries:
            header = binary.read_bytes()[:20]
            if header[:5] != b'\x7fELF\x02' or int.from_bytes(header[18:20], 'little') != elf:
                raise RuntimeError('non-native bootstrap binary: '+str(binary))
        limbo = tools/'limbo'
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        translator = work/'dis2c'
        run(compiler+['-std=c99', '-O2', '-I'+str(ROOT/'include'),
                     str(ROOT/'utils/dis2c/dis2c.c'), '-o', str(translator)], work, environment)
        cases = 0
        for source in (ROOT/'utils/dis2c/examples/sum.b', ROOT/'utils/dis2c/examples/driver.b',
                       ROOT/'appl/cmd/echo.b'):
            for optimize in (False, True):
                output = work/(source.stem+('-opt' if optimize else '')+'.dis')
                command = [str(limbo), '-I', str(ROOT/'module'), '-I', str(ROOT/'utils/dis2c/examples')]
                if optimize:
                    command += ['-O']
                command += ['-o', str(output), str(source)]
                run(command, work, environment)
                first = output.read_bytes()
                run(command, work, environment)
                if output.read_bytes() != first:
                    raise RuntimeError('nondeterministic Dis output: '+str(source))
                run([str(translator), '--check', str(output)], work, environment)
                generated = work/(output.stem+'.c')
                run([str(translator), '-n', 'compiled_probe', '-o', str(generated), str(output)], work, environment)
                includes = ['-I'+str(ROOT/path) for path in
                            ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
                run(compiler+['-std=c99', '-O1']+includes+['-c', str(generated), '-o', str(work/'probe.o')], work, environment)
                cases += 1
        source = work/'numeric probe.b'
        output = work/'numeric.dis'
        operators = ('+', '-', '*', '/', '%', '&', '|', '^', '<<', '>>', '**')
        for number in range(66):
            operator = operators[number % len(operators)]
            source.write_text('implement Numeric;\n'
                'Numeric: module { calc: fn(a, b: int): int; };\n'
                'calc(a, b: int): int { return (a '+operator+' b) + '+str(number)+'; }\n')
            run([str(limbo), '-O', '-o', str(output), str(source)], work, environment)
            run([str(translator), '--check', str(output)], work, environment)
            cases += 1
        # Real compiler output makes the current target ABI explicit and reviewable.
        source.write_text('implement Numeric;\n'
            'Numeric: module { calc: fn(a, b: int): int; };\n'
            'calc(a, b: int): int { return a+b; }\n')
        assembly = work/'numeric.s'
        run([str(limbo), '-S', '-o', str(assembly), str(source)], work, environment)
        text = assembly.read_text()
        if '36(fp),32(fp),0(16(fp))' not in text:
            raise RuntimeError('unexpected legacy argument/result layout\n'+text)
        invalid = ('implement Broken;\n',
                   'implement Numeric; Numeric: module { calc: fn(): int; }; calc(): int { return "bad"; }',
                   'implement Numeric; include "missing-module-for-bootstrap.m";')
        for text in invalid:
            source.write_text(text)
            run([str(limbo), '-o', str(output), str(source)], work, environment, False)
        installed = fingerprints(binaries)
        run(['sh', str(ROOT/'makemk.sh')], work,
            dict(environment, MKBOOTSTRAP_LIMBO='invalid'), False)
        # Fail only the final compiler link, after all native objects/tools are built.
        reject = work/'reject-final-link'
        reject.write_text('#!/bin/sh\nfor argument do\n'
            'case "$argument" in */limbo.native) exit 73;; esac\ndone\n'
            'exec '+shlex.join(compiler)+' "$@"\n')
        reject.chmod(0o755)
        run(['sh', str(ROOT/'makemk.sh')], work, dict(environment, CC=str(reject)), False)
        if fingerprints(binaries) != installed or list(tools.glob('*.new.*')):
            raise RuntimeError('failed native compiler build changed installed tools')
    if fingerprints(protected) != before:
        raise RuntimeError('native compiler bootstrap modified protected source/configuration')
    print('PASS native '+arch+' Limbo: four ELF64 tools, '+str(cases)+
          ' real-source Dis/AOT cases, CLI failures, exact ABI32 offsets and late-link preservation')


# Report compiler, generated-image, native-object and installation failures to the caller.
if __name__ == '__main__':
    main()
