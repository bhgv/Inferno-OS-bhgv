#!/usr/bin/env python3
"""Reject partial caller-guard builds by relinking actual classic-mk objects and archives."""
from pathlib import Path
import argparse
import os
import shlex
import shutil
import subprocess
import tempfile


# Select exact owning compile/link invocations from the caller's classic-mk build log.
def invocation(lines, predicate):
    found = []
    for line in lines:
        try:
            arguments = shlex.split(line)
        except ValueError:
            continue
        if arguments and predicate(arguments):
            found.append(arguments)
    if not found:
        raise RuntimeError('Required coherent owning invocation was absent from build log')
    return found[-1]


# Replace only explicit compiler output, preserving every unrelated target/library option.
def output(arguments, destination):
    result = list(arguments)
    if '-o' in result:
        result[result.index('-o')+1] = str(destination)
    else:
        result += ['-o', str(destination)]
    return result


# Removing any one owning profile must fail its marker, after successful baseline linkage.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--ar', required=True, help='Exact target archive tool, e.g. aarch64-linux-gnu-ar')
    args = parser.parse_args()
    root = args.root.resolve()
    lines = args.log.read_text().splitlines()
    link = invocation(lines, lambda argv: '-o' in argv and argv[argv.index('-o')+1] == 'o.emu-g' and '-c' not in argv)
    library = next(arg for arg in link if arg.endswith('/libinterp.a'))
    with tempfile.TemporaryDirectory(prefix='native64-guard-link-', dir=os.environ.get('NATIVE64_TMPDIR', root.parent)) as directory:
        work = Path(directory); env = dict(os.environ, TMPDIR=str(work))
        subprocess.run(output(link, work/'coherent'), cwd=root/'emu/Linux', env=env, check=True, capture_output=True)
        for name, source, symbol in (('stack', 'stack.c', 'native64callerstackguard'),
                                     ('xec', 'xec.c', 'native64callervmguard'),
                                     ('dis', '../port/dis.c', 'native64callerruntime')):
            compile_ = invocation(lines, lambda argv: '-c' in argv and source in argv)
            if '-DDIS_NATIVE64_CALLER_GUARD=1' not in compile_:
                raise RuntimeError('Owning compile was not caller-guard enabled: '+name)
            compile_ = [arg for arg in compile_ if arg != '-DDIS_NATIVE64_CALLER_GUARD=1']
            case = work/name; case.mkdir()
            obj = case/(name+'.o')
            subprocess.run(output(compile_, obj), cwd=root/('emu/Linux' if name == 'dis' else 'libinterp'), env=env, check=True, capture_output=True)
            controlled = list(link)
            if name == 'dis':
                controlled[controlled.index('dis.o')] = str(obj)
            else:
                archive = case/'libinterp.a'; shutil.copyfile(library, archive)
                subprocess.run(shlex.split(args.ar)+['r', str(archive), str(obj)], env=env, check=True, capture_output=True)
                controlled[controlled.index(library)] = str(archive)
            failed = subprocess.run(output(controlled, case/'partial'), cwd=root/'emu/Linux', env=env, capture_output=True, text=True)
            if failed.returncode == 0 or symbol not in failed.stderr:
                raise RuntimeError('Partial runtime linked or missed its owning marker: '+name+'\n'+failed.stderr)
            print('PASS actual partial-build refusal:', name, symbol, flush=True)


if __name__ == '__main__':
    main()
