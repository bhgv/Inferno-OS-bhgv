#!/usr/bin/env python3
"""Execute RV64I/M complete tails with the actual C executor above4GiB under Linux/QEMU."""
from pathlib import Path
import argparse
import os
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Keep encoder/C executor instructions real; bound only process startup, syscall and VM object services.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', required=True, help='Freestanding RV64-capable C compiler command')
    parser.add_argument('--runner', required=True, help='Linux RV64 runner command')
    args=parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    function=runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    body=function((ROOT/'libinterp/xec.c').read_text(), 'int\ncjitexec(')
    source=(ROOT/'libinterp/tests/riscv-dispatch.c').read_text().replace('EXECUTOR_IMPLEMENTATION', body)
    with tempfile.TemporaryDirectory(prefix='riscv-dispatch-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work=Path(directory)
        env=dict(os.environ,TMPDIR=str(work))
        unit=work/'test.c'; unit.write_text(source)
        script=work/'target.ld'
        script.write_text("""SECTIONS {
    . = 0x100010000;
    .text : { *(.text .text.*) }
    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.* .srodata .srodata.* .eh_frame*) }
    . = ALIGN(4096);
    __global_pointer$ = . + 0x800;
    .data : { *(.data .data.*) }
    .sdata : { *(.sdata .sdata.*) }
    .sbss : { *(.sbss .sbss.*) }
    .bss : { *(.bss .bss.* COMMON) }
}
""")
        command=shlex.split(args.cc)+['-O3','-DNDEBUG','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-pie','-mcmodel=medany','-nostdlib','-static','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp')]
        for isa in ('rv64i', 'rv64im', 'rv64gc'):
            for model, flags in [('WORD32', []), ('WORD64', ['-DTEST_WORD64=1'])]:
                target=work/(isa+'-'+model)
                options=['-march='+isa,'-mabi='+('lp64d' if isa == 'rv64gc' else 'lp64')]+flags
                link=[str(unit),str(ROOT/'utils/tests/host64-start.S'),'-Wl,-e,_start','-Wl,-T,'+str(script),'-o',str(target)]
                subprocess.run(command+options+link,env=env,check=True)
                subprocess.run(shlex.split(args.runner)+[str(target)],env=env,check=True,timeout=30)
                print('PASS actual RV64 complete tails:',isa,model,'173 opcodes, 1038 C/fast outcomes, >4GiB pointers, four-byte entries and code retirement',flush=True)
        for name, old, new in [('return', 'rvi(&buffer, 0x67, 0, Rvzero, Rvt0, 0);', 'rvi(&buffer, 0x67, 0, Rvra, Rvt0, 0);'),
                ('pointer', 'rvput(&buffer, (u32int)(fast>>32));', 'rvput(&buffer, 0);')]:
            header=(ROOT/'libinterp/riscv-emit.h').read_text()
            if header.count(old) != 1: raise RuntimeError('Bridge control boundary changed: '+name)
            (work/'riscv-emit.h').write_text(header.replace(old,new,1))
            # Quoted includes search the source directory before production include roots.
            target=work/('negative-'+name)
            subprocess.run(command+['-march=rv64i','-mabi=lp64']+[str(unit),str(ROOT/'utils/tests/host64-start.S'),'-Wl,-e,_start','-Wl,-T,'+str(script),'-o',str(target)],env=env,check=True)
            result=subprocess.run(shlex.split(args.runner)+[str(target)],env=env,capture_output=True,timeout=30)
            if result.returncode == 0: raise RuntimeError('Bridge mutation was not detected: '+name)
            print('PASS target bridge negative control:',name,flush=True)


if __name__=='__main__':
    main()
