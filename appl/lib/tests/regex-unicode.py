#!/usr/bin/env python3
"""Compile real Regex and run Unicode regressions on a matching hosted Dis VM."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3] # Owning Inferno checkout.


# Run one compiler/VM command with borrowed arguments and environment. Return
# captured output only on success; raise with diagnostics on a failed process.
def run(arguments):
    result = subprocess.run([str(argument) for argument in arguments], text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(str(arguments) + '\n' + result.stdout + result.stderr)
    return result.stdout + result.stderr


# Receive a matching NC64 compiler/runtime pair and optional old-source overrides.
# Own isolated images; execute both dispatch modes/quanta. This optional integration
# test requires no host regexp package and claims no classic/NC64 ABI interchange.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--library', type=Path, default=ROOT / 'appl/lib/regex.b')
    parser.add_argument('--includes', type=Path, default=ROOT / 'module')
    args = parser.parse_args()
    compiler, runtime = args.compiler.resolve(), args.runtime.resolve()
    with tempfile.TemporaryDirectory(prefix='regex Unicode ') as temporary:
        work = Path(temporary)
        run([compiler, '-g', '-I', args.includes.resolve(), '-o', work / 'regex.dis', args.library.resolve()])
        run([compiler, '-g', '-I', args.includes.resolve(), '-o', work / 'check.dis',
             ROOT / 'appl/lib/tests/regex-unicode.b'])
        for mode in ('--jit', '--no-jit'):
            for quantum in (1, 2048):
                output = run([runtime, mode, '--quantum', quantum, '--max-instructions', 2000000, work / 'check.dis'])
                if 'PASS regex Unicode: 85 rune/capture/class checks' not in output or 'FAIL' in output:
                    raise RuntimeError('Regex Unicode matrix failed:\n' + output)
                print(mode, quantum, 'PASS 85 Unicode Regex cases')


if __name__ == '__main__':
    main()
