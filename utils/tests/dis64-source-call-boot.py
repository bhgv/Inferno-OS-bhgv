#!/usr/bin/env python3
"""Boot source-compiled call files in the full console emulator and real OS worker."""
import argparse,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SOURCE=runpy.run_path(str(ROOT/'utils/tests/dis64-source-calls.py'))
BOOT=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ('limbo','emu','root'):p.add_argument('--'+arg,type=Path,required=True)
    p.add_argument('--runner',default='');args=p.parse_args();command=shlex.split(args.runner)+[str(args.emu.resolve()),'-v','-r',str(args.root.resolve())];runs=0
    with tempfile.TemporaryDirectory(prefix='source-call-boot-',dir=args.root.resolve())as td:
        work=Path(td)
        for name,text,kind,value,failure in SOURCE['cases']():
            source=work/(name+'.b');source.write_text(text);image=work/(name+'.dis64')
            SOURCE['TYPED']['READ']['checked_run']([str(args.limbo.resolve()),'-64c','-o',str(image),str(source)],capture_output=True,check=True)
            for jit in (0,1):
                output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d','/'+str(image.relative_to(args.root.resolve()))])
                error='zero divide'if failure else ''
                assert f'native64 scalar module Calls: jit={jit}, exit="{error}"'in output and BOOT['RETIRED']in output,output
                assert 'panic:'not in output and 'faults:'not in output and ('Broken:'in output)==bool(failure),output
                runs+=1;print('PASS source full-emulator boot:',name,'jit='+str(jit),'error='+repr(error),flush=True)
        image=work/'sum-7.dis64';output=BOOT['execute'](command+['-d','/'+str(image.relative_to(args.root.resolve()))])
        assert 'panic: loading 'in output and BOOT['RETIRED']not in output,output
    print('PASS source full-emulator pipeline:',runs,'C/RX OS roots; default profile refusal; no trusted caller metadata',flush=True)
if __name__=='__main__':main()
