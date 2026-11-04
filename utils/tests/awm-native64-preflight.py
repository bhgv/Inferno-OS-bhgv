#!/usr/bin/env python3
"""Compile the AWM startup slice and report the current Dis64 compiler barriers."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STARTUP = (
    'appl/cmd/emuinit.b',
    'appl/wm/awm.b',
    'appl/wm/atoolbar.b',
    'appl/lib/arg.b',
    'appl/lib/string.b',
    'appl/lib/wmsrv.b',
    'appl/lib/wmclient.b',
    'appl/lib/winplace.b',
    'appl/lib/wmlib.b',
    'appl/lib/titlebar.b',
    'appl/lib/tkclient.b',
    'appl/cmd/sh/sh.b',
)


# Capture the real compiler status and diagnostics for a single source.
def compile_module(limbo, source, target, options):
    return subprocess.run(
        [str(limbo), *options, '-I', 'module', '-o', str(target), source],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, timeout=60,
    )


# Keep ordinary source compilation and experimental Dis64 admission separate.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    limbo = args.limbo.resolve()
    refused = 0
    with tempfile.TemporaryDirectory(prefix='awm-preflight-', dir=ROOT) as directory:
        work = Path(directory)
        for source in STARTUP:
            legacy = work / (Path(source).stem + '.dis')
            result = compile_module(limbo, source, legacy, [])
            if result.returncode != 0 or not legacy.is_file() or legacy.stat().st_size == 0:
                raise RuntimeError(f'{source}: ordinary compilation failed\n{result.stdout}')
            target = legacy.with_suffix('.dis64')
            result = compile_module(limbo, source, target, ['-6'])
            if result.returncode == 0:
                if not target.is_file() or target.stat().st_size == 0:
                    raise RuntimeError(f'{source}: Dis64 compiler omitted its output')
                print(f'{source}: emitted Dis64')
                continue
            diagnostic = next((line for line in result.stdout.splitlines()
                               if 'Dis64 scalar:' in line), '')
            if result.returncode < 0 or target.exists() or not diagnostic:
                raise RuntimeError(f'{source}: unexpected Dis64 refusal\n{result.stdout}')
            if source == 'appl/wm/awm.b':
                witness = re.search(r':(\d+): Dis64 scalar: module load requires managed linkage', diagnostic)
                if not witness or 'load ' not in (ROOT/source).read_text().splitlines()[int(witness[1])-1]:
                    raise RuntimeError(f'{source}: missing load-site diagnostic\n{result.stdout}')
            print(diagnostic)
            refused += 1
    print(f'PASS AWM compiler preflight: {len(STARTUP)} ordinary modules, {refused} Dis64 refusals')


if __name__ == '__main__':
    main()
