#!/usr/bin/env python3
"""Compile both real dial implementations with boundary native I/O adapters."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


# Compile unchanged production function bodies against bounded native adapters.
def main():
    """Run active predicates with address/undefined sanitizers and NDEBUG."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', help='test an earlier git source without modifying the checkout')
    parser.add_argument('--case', choices=('parse', 'translate', 'count', 'listen', 'announce', 'clone', 'write'))
    parser.add_argument('--port', choices=('emu', 'os'))
    parser.add_argument('--profile', choices=('address', 'undefined'))
    args = parser.parse_args()
    fixture = (ROOT / 'emu/port/tests/dial-paths.c').read_text()
    with tempfile.TemporaryDirectory(prefix='inferno-dial-') as temporary:
        for port in ([args.port] if args.port else ('emu', 'os')):
            path = f'{port}/port/dial.c'
            source = subprocess.check_output(['git', 'show', f'{args.revision}:{path}'], cwd=ROOT,
                                             text=True) if args.revision else (ROOT / path).read_text()
            if 'static void\n_dial_string_parse' in source:
                # Normalize only the old private return signature for executable controls.
                source = source.replace('static void\t_dial_string_parse', 'static int\t_dial_string_parse')
                source = source.replace('static void\n_dial_string_parse', 'static int\n_dial_string_parse')
                source = source.replace('\t\tds->rem = p + 1;\n\t}\n}', '\t\tds->rem = p + 1;\n\t}\n\treturn 0;\n}', 1)
            source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
            for profile in ([args.profile] if args.profile else ('address', 'undefined')):
                unit = Path(temporary) / f'{port}-{profile}.c'
                binary = unit.with_suffix('')
                unit.write_text(fixture.replace('/* PRODUCTION_SOURCE */', source))
                subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-DNDEBUG',
                                f'-fsanitize={profile}', '-fno-sanitize-recover=all',
                                '-fno-omit-frame-pointer', str(unit), '-o', str(binary)], check=True)
                environment = dict(os.environ)
                print(f'{port} {profile}', flush=True)
                subprocess.run([str(binary)] + ([args.case] if args.case else []), env=environment, check=True)


if __name__ == '__main__':
    main()
