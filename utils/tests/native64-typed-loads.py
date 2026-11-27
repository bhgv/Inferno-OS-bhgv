#!/usr/bin/env python3
"""Execute source LOAD into MP, then imported calls into owned String tables."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
STR=runpy.run_path(str(ROOT/'utils/tests/native64-typed-string-tables.py'))
EXT=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-external.py'))
INTERFACE='Strings:module{choose:fn(s:string):int;global:fn(s:string):int;};'
SOURCE='''implement Client;include "provider.m";
Client:module{open:fn(path:string);run:fn(s:string):int;forward:fn(s:string):int;clear:fn();};
p:Strings;calls:int=17;
open(path:string){p=load Strings path;}
run(s:string):int{calls++;a:=p->choose(s);b:=p->global(s);return a+b;}
forward(s:string):int{return p->choose(s);}
clear(){p=nil;}
'''

def generate(limbo,work):
    # The consumer is compiled independently against its .m declarations.
    data,rows=EXT['compile_source'](limbo,work,SOURCE,interface=INTERFACE,loads=True)
    probe,_=EXT['compile_source'](limbo,work,'implement Probe;include "provider.m";Probe:module{loader:fn():Strings;};loader():Strings{return load Strings "trusted/Strings";}',interface=INTERFACE)
    assert len(rows)==4 and all(r['accepted'] for r in rows),[(r['name'],r.get('reason'))for r in rows]
    assert EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]==EXT['BASE']['READ']['legacy_instructions'](probe,with_imports=True)[-1]
    imports=EXT['BASE']['READ']['legacy_instructions'](probe,with_imports=True)[-1]
    assert len(imports)==1 and len(imports[0])==2;imports=imports[0]
    exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1]
    exports={e['name']:e for e in exports}
    lines=['#include "native64-typed-module.h"','static Modlink *trustedload(Module*,WORD,char*);'];prototypes={}
    for r in rows:
        if r.get('externals'):
            EXT['verify'](data,r,imports,loads=True)
            assert all(e['module'][0]==0 for e in r['externals'])
            prototypes.update({e['name']:e for e in r['externals']})
        else:EXT['verify'](data,r,imports,loads=True)
        n=r['name'];sites=r.get('externals',[])
        lines += [f'static const Native64TypedStep client_code_{n}[]={{']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};',f'static const uchar client_map_{n}[]={{'+','.join(map(str,r['frame']['map']))+'};']
        if sites:lines += [f'static const Native64ModuleSite client_sites_{n}[]={{']+['{'+','.join(map(str,(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])))+'},'for e in sites]+['};']
    original=next(r for r in rows if r['name']=='run')
    for fault in ('module','binding','contract','suffix'):
        damaged=json.loads(json.dumps(original))
        if fault=='module':damaged['externals'][0]['module'][1]+=8
        if fault=='binding':damaged['externals'][0]['binding']^=1
        if fault=='contract':damaged['externals'][0]['contract']^=16
        if fault=='suffix':damaged['code'][-2][4]+=8
        try:EXT['verify'](data,damaged,imports,loads=True)
        except (AssertionError,KeyError,IndexError):pass
        else:raise AssertionError('LOAD corrupt report accepted: '+fault)
    requests=[]
    for i,entry in enumerate(imports):
        e=prototypes[entry['name']];assert e['signature']==entry['signature'] and e['args']==[[64,13,4,8]]
        lines.append(f'static const uchar import_map_{i}[]={{0,128}};')
        requests.append('{'+json.dumps(entry['name'])+f',{entry["signature"]}U,72,import_map_{i},2'+'}')
    lines+=['static const Native64ModuleImport client_requests[]={'+','.join(requests)+'};','static const Native64ModuleTable client_tables[]={{client_requests,2}};','static const Native64ModuleFunction client_functions[]={']
    for r in rows:
        n=r['name'];e=exports[n];assert e['pc']==r['first'];sites=r.get('externals',[])
        lines.append('{'+json.dumps(n)+f',{e["signature"]}U,1,client_code_{n},{len(r["code"])},{r["frame"]["bytes"]},client_map_{n},{len(r["frame"]["map"])},'+(f'client_sites_{n}' if sites else 'nil')+f',{len(sites)}'+'},')
    r=rows[0];globals={g[0]:g[1]for g in r['globals']};assert set(globals)=={'p','calls','nil'}
    initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3]
    oldcalls=next(leaf[0]for leaf in r['data']['leaves']if leaf[1]==globals['calls'])
    assert initial=={oldcalls:{'kind':'word','low':17,'high':0}},initial
    lines+=['};','static const uchar client_data_map[]={'+','.join(map(str,r['data']['map']))+'};','static const Native64ModuleInit client_initial[]={{'+str(globals['calls'])+',4,17}};',
            'static const Native64ModuleSpec client_spec={"Client","trusted/Client",client_functions,4,~0U,'+f'{r["data"]["bytes"]},client_data_map,{len(r["data"]["map"])},client_initial,1,client_tables,1,nil,nil,0,trustedload'+'};',
            f'static const unsigned client_provider_offset={globals["p"]},client_calls_offset={globals["calls"]};']
    # Function numbers follow source order, avoiding assumed export/PC ordering.
    lines += [f'static const unsigned client_{r["name"]}={i};'for i,r in enumerate(rows)]
    (work/'typed-loads.h').write_text('\n'.join(lines)+'\n')
    original_open=next(r for r in rows if r['name']=='open')
    assert original_open['schema']=='inferno-typed-load-v1' and original_open['code'][0][0]==8
    for field in (1,2,3,4):
        damaged=json.loads(json.dumps(original_open));damaged['code'][0][field]^=8 if field in (2,4) else 1
        try:EXT['verify'](data,damaged,imports,loads=True)
        except (AssertionError,KeyError,IndexError):pass
        else:raise AssertionError('corrupt LOAD report accepted: '+str(field))
    # The existing external mode must keep refusing this complete LOAD body.
    _,oldrows=EXT['compile_source'](limbo,work,SOURCE,interface=INTERFACE)
    assert not next(r for r in oldrows if r['name']=='open')['accepted']
    multi='implement Two;include "provider.m";Other:module{size:fn(n:int):int;};Two:module{a:fn(path:string,s:string):int;b:fn(path:string):int;};p:Strings;q:Other;a(path:string,s:string):int{p=load Strings path;return p->choose(s);}b(path:string):int{q=load Other path;return q->size(3);}'
    twodata,tworows=EXT['compile_source'](limbo,work,multi,interface=INTERFACE,loads=True)
    twotables=EXT['BASE']['READ']['legacy_instructions'](twodata,with_imports=True)[-1]
    assert len(tworows)==2 and len(twotables)==2 and all(r['accepted']for r in tworows)
    indices=set()
    for r in tworows:
        index=next(step[3]for step in r['code']if step[0]==8);indices.add(index)
        EXT['verify'](twodata,r,twotables[index],loads=True)
    assert indices=={0,1}
    print('PASS LOAD source: four complete bodies, one LOAD, three AMP calls, independent .m/classic oracles and unchanged old refusal',flush=True)
    print('PASS LOAD source tables: two distinct interfaces retain exact classic table indices 0/1',flush=True)
    return r['data']['bytes']

def unit():
    source=runpy.run_path(str(ROOT/'utils/tests/native64-typed-string-imports.py'))['unit']()
    source=source.replace('#define RuntimeBlocks 256','#define RuntimeBlocks 1024',1)
    extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function'];xec=(ROOT/'libinterp/xec.c').read_text()
    source+='\nulong msize(void *p){for(unsigned i=0;i<RuntimeBlocks;i++)if(blocks[i].pointer==p)return blocks[i].bytes;require(0);return 0;}\n'
    convert=extract((ROOT/'libinterp/string.c').read_text(),'char*\nstring2c(')
    assert convert.count('buffer = malloc(l);')==1
    source+=convert.replace('buffer = malloc(l);','buffer = mallocz(l,0);')+'\n'
    source+='Module *lookmod(char *p){USED(p);require(0);return nil;}\nModule *readmod(char*p,Module*m,int sync){USED(p);USED(m);USED(sync);require(0);return nil;}\n'
    source+=extract(xec,'static Import*\nmoduleimports(')+'\n'+extract(xec,'OP(iload)')+'\n'
    return source

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
    if not __debug__:p.error('independent source oracles require assertions')
    with tempfile.TemporaryDirectory(prefix='typed-loads-',dir=ROOT)as td:
        work=Path(td);STR['generate'](a.limbo.resolve(),work);clientbytes=generate(a.limbo.resolve(),work)
        source=unit()+'\n#define main tablesoldmain\n'+(ROOT/'utils/tests/native64-typed-string-tables.c').read_text()+'\n#undef main\n#line 1 "native64-typed-loads.c"\n'+(ROOT/'utils/tests/native64-typed-loads.c').read_text()
        file=work/'test.c';file.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-I'+str(work)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'test';command=cc+[str(file),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)]
        subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
        production=(ROOT/'include/native64-typed-module.h').read_text()
        checks=[('s->load==nil || s->load==(Modlink*(*)(Module*,WORD,char*))H','0','case=0'),
                ('if((map[bit/8] & (0x80>>(bit%8)))==0)return 0;',';','case=5')]
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
        target=extract(production,'static int\nnative64modulecheck(')
        for before,after,diagnostic in checks:
            assert target.count(before)==1
            (work/'native64-typed-module.h').write_text(production.replace(target,target.replace(before,after,1),1))
            subprocess.run(command,check=True)
            result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
            assert result.returncode==10 and ('LOAD metadata guard omitted: '+diagnostic)in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,result.stderr
            print('PASS LOAD metadata active control:',arch,diagnostic,flush=True)
        (work/'native64-typed-module.h').unlink()
        extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function'];body=extract(source,'OP(iload)');before='ptrdel(D2H(ml));';assert body.count(before)==1
        file.write_text(source.replace(body,body.replace(before,';'),1));subprocess.run(command,check=True)
        result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
        assert result.returncode==10 and 'LOAD publication pin leaked'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,result.stderr
        print('PASS LOAD publication active control:',arch,'pin transfer',flush=True)

if __name__=='__main__':main()
