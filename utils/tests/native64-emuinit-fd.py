#!/usr/bin/env python3
"""Execute checked direct Sys.FD/nil branches without assuming FD payload layout."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
EXT=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-external.py'))
SOURCE='''implement Probe;include "sys.m";
Probe:module{equal:fn(fd:ref Sys->FD):int;different:fn(fd:ref Sys->FD):int;reverse:fn(fd:ref Sys->FD):int;local:fn(fd:ref Sys->FD):int;};
equal(fd:ref Sys->FD):int{if(fd==nil)return 17;return 29;}
different(fd:ref Sys->FD):int{if(fd!=nil)return 29;return 17;}
reverse(fd:ref Sys->FD):int{if(nil==fd)return 17;return 29;}
local(fd:ref Sys->FD):int{v:=fd;if(v==nil)return 17;return 29;}
'''
def generate(limbo,w):
    # compile_source uses provider.m; give it the real Sys declarations.
    source=SOURCE.replace('include "sys.m";','include "provider.m";')
    data,rows=EXT['compile_source'](limbo,w,source,interface=(ROOT/'module/sys.m').read_text(),loads=True)
    assert len(rows)==4 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    old=EXT['BASE']['READ']['legacy_instructions'](data)[1];lines=['#include "native64-typed-code.h"']
    for r in rows:
        checked=json.loads(json.dumps(r));branches=0
        for i,c in enumerate(checked['code']):
            if c[0]in(EXT['OPCODES']['IBEQL'],EXT['OPCODES']['IBNEL']):
                o=old[r['first']+i];assert o['op']in(EXT['OPCODES']['IBEQW'],EXT['OPCODES']['IBNEW'])
                assert c[0]==(EXT['OPCODES']['IBEQL']if o['op']==EXT['OPCODES']['IBEQW']else EXT['OPCODES']['IBNEL'])
                assert c[1]==(8|192|2) and c[5]==0
                assert any(l[1]==c[2]and l[3:]==[8,12]for l in r['frame']['leaves'])
                assert any(l[1]==c[3]and l[3]==8 for l in r['data']['leaves'])
                maps={space:{l[0]:l[1]for l in r[space]['leaves']}for space in ('frame','data')}
                sm=o['mode']>>3&7;mm=o['mode']&192
                assert sm in(0,1)and mm in(128,192)
                lhs=maps['frame'if sm else'data'][o['s'][0]];rhs=maps['frame'if mm==128 else'data'][o['m']]
                assert (c[2],c[3])==((lhs,rhs)if sm==1 else(rhs,lhs))
                assert c[4]==o['d'][0]-r['first']
                c[:4]=[o['op'],o['mode'],lhs,rhs];branches+=1
        assert branches==1
        EXT['verify'](data,checked,[],loads=True,rows=rows)
        n=r['name'];lines+=['static const Native64TypedStep fd_'+n+'[]={']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};','static const uchar fd_map_'+n+'[]={'+','.join(map(str,r['frame']['map']))+'};']
    lines+=['typedef struct FDBody{const Native64TypedStep*code;unsigned count,bytes,arg;const uchar*map;}FDBody;','static const FDBody fd_bodies[]={']+['{fd_'+r['name']+f',{len(r["code"])},{r["frame"]["bytes"]},{r["args"][0][0]},fd_map_'+r['name']+'},'for r in rows]+['};',f'static const unsigned fd_databytes={rows[0]["data"]["bytes"]};','static const uchar fd_datamap[]={'+','.join(map(str,rows[0]['data']['map']))+'};']
    (w/'fd.h').write_text('\n'.join(lines)+'\n')
    # Equality of two references is deliberately outside this nil-only profile.
    bad=source.replace('if(fd==nil)','if(fd==other)').replace('equal(fd:ref Sys->FD):int{','equal(fd:ref Sys->FD):int{other:=fd;')
    _,refused=EXT['compile_source'](limbo,w,bad,interface=(ROOT/'module/sys.m').read_text(),loads=True)
    assert not refused[0]['accepted']and refused[0]['reason']=='load-nil-roles'
    print('PASS real Sys.FD source: four complete bodies, independent classic pointer-width relocation and ref/ref refusal',flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
    if not __debug__:p.error('source proofs require assertions')
    runpy.run_path(str(ROOT/'utils/tests/dis64-ref-nil.py'))['main']()
    with tempfile.TemporaryDirectory(prefix='emuinit-fd-',dir=ROOT)as td:
        w=Path(td);generate(a.limbo.resolve(),w)
        frontier=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](a.limbo.resolve(),'appl/cmd/emuinit.b',w)
        getenv=next(f for f in frontier['functions']if f['name']=='getenv')
        assert not getenv['accepted']and getenv['reason']=='unsupported-opcode'and getenv['instruction']['opcode']=='INEWA'
        print('PASS unchanged real emuinit frontier: FD/nil and Sys.fstat passed; remaining getenv refusal is byte-array allocation (no complete startup execution)',flush=True)
        source=runpy.run_path(str(ROOT/'utils/tests/native64-local-calls.py'))['unit']();source=source[:source.index('/* Keep scalar file admission separate')]
        source=source.replace('require(int condition)','requireline(int condition,int line)',1).replace('after %d predicates\\n", checks','after %d predicates at generated line %d\\n", checks,line',1).replace('checks++;\n}', 'checks++;\n}\n#define require(c) requireline(!!(c),__LINE__)',1)
        source+='\n#line 1 \"native64-emuinit-fd.c\"\n'+(ROOT/'utils/tests/native64-emuinit-fd.c').read_text();file=w/'unit.c';file.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'));arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=w/'unit';release=runpy.run_path(str(ROOT/'utils/tests/native64-typed-files.py'))['releaseflags']
        cc+=[str(file),str(ROOT/'libinterp/dec.c')]+release(cc)+['-lm','-o',str(binary)]
        subprocess.run(cc,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=120)
if __name__=='__main__':main()
