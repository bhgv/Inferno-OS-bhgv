#!/usr/bin/env python3
"""Keep old Sys files equal and selected mount files deterministic without changing classic Dis."""
import argparse,runpy,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo',required=True,type=Path)
    parser.add_argument('--baseline',required=True,type=Path)
    a=parser.parse_args();mount=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-mount.py'));base=mount['BASE'];old=0
    with tempfile.TemporaryDirectory(prefix='mount-producer-',dir=ROOT)as name:
        work=Path(name)
        for family in ('string','groups','fixed','dial','connection'):
            source=runpy.run_path(str(ROOT/('utils/tests/dis64-sys-'+family+'.py')))
            new=source['rows'](a.limbo,work);before=source['rows'](a.baseline,work)
            if len(new)!=len(before):raise RuntimeError('old family count changed')
            for i,(left,right)in enumerate(zip(new,before)):
                images=lambda row:[value for value in row if isinstance(value,bytes)]
                if images(left)!=images(right):raise RuntimeError((family,i,'old Dis64 bytes changed'))
            old+=len(new)
        programs=mount['CASES']['programs']()
        for index,(label,body,_,_,_) in enumerate(programs):
            text='sys:=load Sys Sys->PATH;'+body
            first=base['compile_source'](a.limbo,work,text)
            if first!=base['compile_source'](a.limbo,work,text):raise RuntimeError(label+' nondeterministic')
            if index<9:
                if first!=base['compile_source'](a.baseline,work,text):raise RuntimeError(label+' prior mount bytes changed')
                old+=1
            else:base['compile_source'](a.baseline,work,text,accepted=False)
            classic=base['compile_source'](a.limbo,work,text,flag='-i')
            if classic!=base['compile_source'](a.baseline,work,text,flag='-i'):raise RuntimeError(label+' classic bytes changed')
        mount['compiler_refusals'](a.limbo,work)
    print('PASS mount producer:',old,'prior Sys images equal; 6 deterministic new sources, baseline refusals, classic bytes equal; 4 well-typed source refusals')


if __name__=='__main__':main()
