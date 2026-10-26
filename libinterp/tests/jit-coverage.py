#!/usr/bin/env python3
"""Count published leaf opcode paths by invoking the production sizing emitters.

This is opcode availability, not proof that every operand reaches native code.
Guards may decline to the interpreter, and native 64-bit VM ABI work is separate.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def opnames():
    """Read the real opcode enum, keeping reserved instruction slots visible."""
    source = (ROOT/'include/isa.h').read_text().split('/* Fix MAXDIS')[0]
    source = re.sub(r'/\*.*?\*/', '', source, flags=re.S)
    return re.findall(r'\bI[A-Z][A-Z0-9]*\b', source)


def measure(arch, wordbytes, profile, names, directory):
    """Ask an actual backend which opcodes fit its production leaf capacity."""
    rv = arch == 'riscv64'
    fields = dict(pc=0, s=8, d=16, m=24, fp=32, arraylen=8,
                  arraydata=32, stringlen=12, stringdata=32, listdata=24,
                  listtail=8, fixedtmp=28, fixedcoef=20, arraytype=16,
                  typesize=24, framelr=0, framefp=8, module=40,
                  linkmodule=8, moduleprog=16, modulenprog=24, instbytes=24,
                  wordbytes=wordbytes)
    header = 'riscv-emit.h' if rv else arch+'-emit.h'
    if 'int heaptype;' in (ROOT/'libinterp'/('riscv-emit.h' if rv else 'leaf64-ops.h')).read_text():
        fields['heaptype'] = 24 if rv else 16
    if not rv and 'int linkcompiled;' in (ROOT/'libinterp/leaf64-ops.h').read_text():
        fields['linkcompiled']=16
    if not rv:
        fields.update(heapref=24,heapcolor=32,refbytes=8,mutator='(void*)0x100008000ULL',nprop='(void*)0x100008008ULL',markcolor=3,moduletypes=32,modulenty=40,typenp=28,stringmax=4,sp=48,ts=56,frametype=16,framemr=24,framesize=32,linknlinks=48,linklinks=56,modlbytes=16,modlframe=8,regmp=64,regtemp=72,linkprog=24,linkmp=0,modlpc=0,arrayroot=24,typeref=0,stringtype="(void*)0x100008010ULL",arrayidentity="(void*)0x100008020ULL",typefree=8,typemark=16,plainfree="(void*)0x100008030ULL",plainmark="(void*)0x100008040ULL")
    if rv:
        fields.update(xlen=64, hasm=int(profile != 'i'), hasdiv=int(profile == 'm'))
    assignments = ''.join(f'abi.{key}={value};' for key, value in fields.items())
    source = directory/'coverage.c'
    source.write_text('#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\n'
                      'typedef uint32_t u32int;typedef unsigned char uchar;\n#define nil 0\n'
                      '#include "isa.h"\n#include "'+header+'"\n'
                      '/* Query every opcode with a valid private layout. */\n'
                      'int main(void){'+('RvLayout' if rv else 'LeafLayout')+' abi={0};'
                      +assignments+'int i,n;printf("%d\\n",MAXDIS);for(i=0;i<MAXDIS;i++){n='
                      +('rvemit(0,RvMax' if rv else 'leafemit(0,LeafMax')
                      +',&abi,i);if(n>0)printf("%d\\n",i);}return 0;}\n')
    binary = directory/'coverage'
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1'))
    subprocess.run(command+['-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'),
                            str(source), '-o', str(binary)], check=True)
    output = [int(value) for value in subprocess.check_output([str(binary)], text=True).split()]
    assert output[0] == len(names), 'opcode enum parsing disagrees with MAXDIS'
    indices = output[1:]
    assert all(0 <= value < len(names) for value in indices)
    available = [names[value] for value in indices]
    remaining = [name for name in names if name not in available]
    return dict(architecture=arch, wordbits=wordbytes*8, profile=profile,
                total=len(names), active_total=len(names)-2, with_native_path=len(available),
                active_interpreter_only=len([name for name in remaining if name not in ('INOP','IECLR')]),
                interpreter_only=len(remaining), available=available, remaining=remaining)


def main():
    """Print a compact comparison or machine-readable evidence for a checkpoint."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    names = opnames()
    records = []
    with tempfile.TemporaryDirectory(prefix='jit-coverage-') as directory:
        for arch in ('amd64', 'arm64', 'riscv64'):
            for wordbytes in (4, 8):
                for profile in (('i', 'zmmul', 'm') if arch == 'riscv64' else ('base',)):
                    records.append(measure(arch, wordbytes, profile, names, Path(directory)))
    if args.json:
        print(json.dumps(dict(reserved=['INOP', 'IECLR'], records=records), indent=2))
    else:
        print('architecture WORD profile active-total native-path active-interpreter-only')
        for row in records:
            print(row['architecture'], row['wordbits'], row['profile'], row['active_total'],
                  row['with_native_path'], row['active_interpreter_only'])
        print('INOP is invalid; IECLR is unused. Both enum slots are excluded from active counts.')
        print('A native path may be guarded/partial; this is not full semantic coverage.')


if __name__ == '__main__':
    main()
