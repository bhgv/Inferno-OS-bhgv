#!/usr/bin/env python3
"""Build native host tools and compile the actual Limbo compiler, without target ABI changes."""
import hashlib
import os
from pathlib import Path
import platform
import shlex
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]  # Owning source tree.
STRICT = '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all -Werror=implicit-function-declaration -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast'


# Keep failures visible while avoiding the unsupported process-based leak scanner.
def run(command, cwd, env, success=True):
    result = subprocess.run(command, cwd=cwd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != success:
        raise RuntimeError('command: '+shlex.join(map(str, command))+'\n'+result.stdout)
    return result.stdout


# Compare existing tools and configuration without modifying either.
def fingerprints(paths):
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None for p in paths}


# Exercise the native bootstrap, rejected compilers, real recipes and real grammar.
def main():
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'aarch64'):
        raise RuntimeError('this integration driver requires a native Linux x64 or arm64 host')
    arch = 'amd64' if platform.machine() == 'x86_64' else 'arm64'
    protected = [ROOT/'mkconfig', ROOT/'Linux/386/bin/mk', ROOT/'Linux/386/bin/iyacc']
    before = fingerprints(protected)
    env = dict(os.environ, MKHOSTOS='Linux', MKHOSTARCH=arch, ROOT=str(ROOT), INFERNO_ROOT=str(ROOT))
    env['CFLAGS'] = os.environ.get('NATIVE_BOOTSTRAP_CFLAGS', STRICT)
    with tempfile.TemporaryDirectory(prefix='inferno native tools ') as directory:
        work = Path(directory)
        tools = work/'native bin'
        env['MKBOOTSTRAP_BINDIR'] = str(tools)
        run(['sh', str(ROOT/'makemk.sh')], ROOT, env)
        binaries = [tools/'mk', tools/'iyacc']
        for binary in binaries:
            header = binary.read_bytes()[:20]
            machine = int.from_bytes(header[18:20], 'little')
            if header[:5] != b'\x7fELF\x02' or machine != (62 if arch == 'amd64' else 183):
                raise RuntimeError('bootstrap installed a non-native ELF binary: '+str(binary))
        installed = fingerprints(binaries)
        run(['sh', str(ROOT/'makemk.sh')], ROOT, dict(env, CC='/bin/false'), False)
        wrong = 'arm64' if arch == 'amd64' else 'amd64'
        run(['sh', str(ROOT/'makemk.sh')], ROOT, dict(env, MKHOSTARCH=wrong), False)
        if installed != fingerprints(binaries) or list(tools.glob('*.new.*')):
            raise RuntimeError('failed bootstrap changed previously installed tools')
        (work/'input').write_text('native-mk-ok\n')
        # mk deliberately treats equal second-resolution timestamps as out of date.
        old_time = time.time()-5
        os.utime(work/'input', (old_time, old_time))
        (work/'rules').write_text('COPY=cp\n')
        (work/'mkfile').write_text('<rules\nall:V: output\n\tcat output\noutput: input\n\t$COPY $prereq $target\n')
        output = run([str(tools/'mk')], work, env)
        if 'native-mk-ok' not in output or (work/'output').read_text() != 'native-mk-ok\n':
            raise RuntimeError('native mk failed variable/include/dependency recipe')
        original_time = (work/'output').stat().st_mtime_ns
        run([str(tools/'mk')], work, env)
        if (work/'output').stat().st_mtime_ns != original_time:
            raise RuntimeError('native mk rebuilt an unchanged dependency')
        (work/'bad.mk').write_text('all:V:\n\tfalse\n')
        run([str(tools/'mk'), '-f', 'bad.mk'], work, env, False)
        parser = work/'parser'
        parser.mkdir()
        run([str(tools/'iyacc'), '-d', str(ROOT/'limbo/limbo.y')], parser, env)
        if not (parser/'y.tab.c').is_file() or not (parser/'y.tab.h').is_file():
            raise RuntimeError('native iyacc omitted Limbo parser output')
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        flags = shlex.split(env['CFLAGS'])
        includes = ['-I'+str(p) for p in (parser, ROOT/'Linux/386/include', ROOT/'include', ROOT/'limbo')]
        sources = sorted((ROOT/'limbo').glob('*.c')) + [parser/'y.tab.c']
        for source in sources:
            run(compiler+flags+includes+['-DINCPATH="'+str(ROOT/'module')+'"', '-c', str(source), '-o', str(parser/(source.stem+'.o'))], parser, env)
        summary = 'PASS native '+arch+' tools: ELF64 mk/iyacc, two rejected compilers preserve binaries, recipes/includes/incremental/failure, real Limbo grammar and '+str(len(sources))+' compiler units'
    if before != fingerprints(protected):
        raise RuntimeError('bootstrap changed existing configuration or 386 tools')
    print(summary)



# Return a nonzero status for any build, runtime, preservation or cleanup failure.
if __name__ == '__main__':
    main()
