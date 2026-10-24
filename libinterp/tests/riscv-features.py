#!/usr/bin/env python3
"""Compile actual RISC-V feature selection for I, Zmmul and M profiles."""
import ctypes
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'libinterp/comp-riscv.c').read_text()
fragment=source[source.index('#ifdef __riscv_mul'):source.index('\tabi.pc=O(REG, PC);')]
unit='struct {int hasm,hasdiv;} abi;\nint features(void){\n'+fragment+'return abi.hasm+2*abi.hasdiv;}\n'
with tempfile.TemporaryDirectory(prefix='riscv-features-') as directory:
    td=Path(directory)
    (td/'features.c').write_text(unit)
    for definitions, expected in (([],0),(['__riscv_mul'],1),(['__riscv_mul','__riscv_div'],3)):
        binary=td/('features'+str(expected)+'.so')
        command=shlex.split(os.environ.get('CC','cc'))
        command+=['-shared','-fPIC','-U__riscv_mul','-U__riscv_div']
        command+=['-D'+macro+'=1' for macro in definitions]
        subprocess.run(command+[str(td/'features.c'),'-o',str(binary)],check=True)
        assert ctypes.CDLL(str(binary)).features()==expected
print('PASS RISC-V compiler feature profiles: I, Zmmul, M')
