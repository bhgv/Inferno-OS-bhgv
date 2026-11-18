#!/usr/bin/env python3
"""Prove all real emuinit variadic argument plans against unchanged classic bytes.

These are private trusted C plans, not checked-body or serialized call admission.
"""
import argparse,json,os,runpy,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
V=runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))

def callers(data):
    """Find callable entry/descriptor pairs from actual classic FRAME/CALL pairs."""
    header,code,types=V['legacy_instructions'](data,with_types=True)
    entries={header[7]:header[8]}
    for pc,call in enumerate(code):
        if call['op']!=4 or call['mode']!=10:continue
        frame=next((old for old in reversed(code[:pc])if old['op']==5 and old['d']==call['s']),None)
        if frame is None or frame['mode']!=17 or frame['s'][0]not in types:raise RuntimeError('Local CALL has no descriptor FRAME')
        entry=call['d'][0];descriptor=frame['s'][0]
        if not 0<=entry<len(code) or entries.get(entry,descriptor)!=descriptor:raise RuntimeError('Conflicting local caller identity')
        entries[entry]=descriptor
    return entries

def generate(limbo,work,env=None):
    command=[str(limbo),'-i','-I',str(ROOT/'module')]
    source=ROOT/'appl/cmd/emuinit.b';plain=work/'emuinit.dis';trace=work/'emuinit-trace.dis'
    V['checked_run'](command+['-o',str(plain),str(source)],check=True,capture_output=True,env=env)
    output=V['checked_run'](command+['-Dq','-o',str(trace),str(source)],check=True,capture_output=True,text=True,env=env)
    data=plain.read_bytes();assert data==trace.read_bytes()
    records=[json.loads(l.split(' ',1)[1])for l in output.stderr.splitlines()if l.startswith('NATIVE64_CALL ')]
    assert [r['callee']for r in records]==['Sys_fprint','Sys_sprint','Sys_fprint']
    assert [r['frame_bytes']for r in records]==[88,72,96]
    positions=V['verify_locations'](data,records,output.stderr,callers=callers(data))
    programs=V['verify_stores'](data,records,output.stderr)
    assert [positions[r['id']]['caller_pc']for r in records]==[0,56,56]
    assert [len(programs[r['id']]['cleanups'])for r in records]==[1,0,1]
    assert [r['map']for r in records]==[[0,224],[0,128],[0,240]]
    for fault in ('missing-clear','duplicate-clear','clear-is-store','source','caller'):
        lines=output.stderr.splitlines();altered=[]
        for line in lines:
            if line.startswith('NATIVE64_CALL_LOWER '):
                tag,payload=line.split(' ',1);x=json.loads(payload)
                if x['id']==records[0]['id']:
                    if fault=='missing-clear':x['cleanups']=[]
                    elif fault=='duplicate-clear':x['cleanups']*=2
                    elif fault=='clear-is-store':x['cleanups']=[x['stores'][0]['legacy_pc']]
                    elif fault=='source':x['stores'][0]['source_value']+=4
                line=tag+' '+json.dumps(x)
            if fault=='caller'and line.startswith('NATIVE64_CALL_PC '):
                tag,payload=line.split(' ',1);x=json.loads(payload)
                if x['id']==records[1]['id']:x['caller_descriptor']=positions[records[0]['id']]['caller_descriptor']
                line=tag+' '+json.dumps(x)
            altered.append(line)
        diagnostic='\n'.join(altered)
        try:
            V['verify_locations'](data,records,diagnostic,callers=callers(data));V['verify_stores'](data,records,diagnostic)
        except (RuntimeError,KeyError,IndexError):pass
        else:raise RuntimeError('Real emuinit oracle missed '+fault)
    header=[]
    for i,r in enumerate(records):
        program=programs[r['id']];label='emuinit_call_'+str(i)
        header+=V['emit_program'](label,program)
        header+=['static const uchar '+label+'_framemap[]={'+','.join(map(str,r['map']))+'};',
                 'static const unsigned int '+label+'_framebytes='+str(r['frame_bytes'])+';']
        if program['cleanups']:
            clear=program['cleanups'][0];prior=[j for j,store in enumerate(program['stores'])if store['legacy_pc']<clear]
            assert prior==[0]
            header+=['static const unsigned int '+label+'_clearcell='+str(program['stores'][0]['source_cell'])+';']
    (work/'emuinit-varargs-native64.h').write_text('\n'.join(header)+'\n')
    print('PASS unchanged emuinit variadic plans: two fprint owner cleanups, empty sprint tail, exact 88/72/96-byte frames, init/loadmod identities and five damaged-report refusals; runtime admission unchanged',flush=True)
    return records,programs

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
    if not __debug__:p.error('source oracles require assertions')
    with tempfile.TemporaryDirectory(prefix='emuinit-varargs-',dir=ROOT)as td:generate(a.limbo.resolve(),Path(td),dict(os.environ))
if __name__=='__main__':main()
