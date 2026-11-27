#!/usr/bin/env python3
"""Execute source imports through module roots in MP into owned String tables."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
STR=runpy.run_path(str(ROOT/'utils/tests/native64-typed-string-tables.py'))
EXT=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-external.py'))
INTERFACE='Strings:module{choose:fn(s:string):int;global:fn(s:string):int;};'
SOURCE='''implement Client;include "provider.m";
Client:module{bind:fn(p:Strings);run:fn(s:string):int;forward:fn(s:string):int;clear:fn();};
p:Strings;calls:int=17;
bind(next:Strings){p=next;}
run(s:string):int{calls++;a:=p->choose(s);b:=p->global(s);return a+b;}
forward(s:string):int{return p->choose(s);}
clear(){p=nil;}
'''

def generate(limbo,work):
    # The consumer is compiled independently against its .m declarations.
    data,rows=EXT['compile_source'](limbo,work,SOURCE,interface=INTERFACE)
    probe,_=EXT['compile_source'](limbo,work,'implement Probe;include "provider.m";Probe:module{loader:fn():Strings;};loader():Strings{return load Strings "trusted/Strings";}',interface=INTERFACE)
    assert len(rows)==4 and all(r['accepted'] for r in rows),[(r['name'],r.get('reason'))for r in rows]
    assert EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]==[]
    imports=EXT['BASE']['READ']['legacy_instructions'](probe,with_imports=True)[-1]
    assert len(imports)==1 and len(imports[0])==2;imports=imports[0]
    exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1]
    exports={e['name']:e for e in exports}
    lines=['#include "native64-typed-module.h"'];prototypes={}
    for r in rows:
        if r.get('externals'):
            EXT['verify'](data,r,imports)
            assert all(e['module'][0]==0 for e in r['externals'])
            prototypes.update({e['name']:e for e in r['externals']})
        else:EXT['BASE']['verify'](data,r)
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
        try:EXT['verify'](data,damaged,imports)
        except (AssertionError,KeyError,IndexError):pass
        else:raise AssertionError('MP String corrupt report accepted: '+fault)
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
            'static const Native64ModuleSpec client_spec={"Client","trusted/Client",client_functions,4,~0U,'+f'{r["data"]["bytes"]},client_data_map,{len(r["data"]["map"])},client_initial,1,client_tables,1'+'};',
            f'static const unsigned client_provider_offset={globals["p"]},client_calls_offset={globals["calls"]};']
    # Function numbers follow source order, avoiding assumed export/PC ordering.
    lines += [f'static const unsigned client_{r["name"]}={i};'for i,r in enumerate(rows)]
    (work/'typed-string-imports.h').write_text('\n'.join(lines)+'\n')
    print('PASS String MP imports source: four complete bodies, three AMP call sites, independent .m hashes/maps and ordinary MP initializer',flush=True)
    return r['data']['bytes']

def unit():
    source=STR['DATA']['unit']().split('/* Actual constructor, String factory and mapped MP cloning; no AWM code body. */')[0]
    extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function'];link=(ROOT/'libinterp/link.c').read_text()
    source+='\ntypedef struct LinkModule LinkModule;\n'+extract(link,'struct LinkModule {')+';\n'
    source+='void newdyndata(Modlink *m){USED(m);require(0);}\n'
    for sig in ('static int\nlinkmodule(','Modlink*\nlinkmod('):source+=extract(link,sig)+'\n'
    source+=extract((ROOT/'libinterp/string.c').read_text(),'OP(lenc)')+'\n'
    return source

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
    if not __debug__:p.error('independent source oracles require assertions')
    with tempfile.TemporaryDirectory(prefix='typed-string-imports-',dir=ROOT)as td:
        work=Path(td);STR['generate'](a.limbo.resolve(),work);clientbytes=generate(a.limbo.resolve(),work)
        source=unit()+'\n#define main tablesoldmain\n'+(ROOT/'utils/tests/native64-typed-string-tables.c').read_text()+'\n#undef main\n#line 1 "native64-typed-string-imports.c"\n'+(ROOT/'utils/tests/native64-typed-string-imports.c').read_text()
        file=work/'test.c';file.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-I'+str(work)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'test';command=cc+[str(file),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)]
        subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
        # Both omissions must fail semantic predicates before any unsafe teardown.
        extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
        production=(ROOT/'include/native64-typed-module.h').read_text()
        before='bit=site->moduleoffset/8;if(!(map[bit/8]&(0x80>>(bit%8))))return 0;';assert production.count(before)==1
        (work/'native64-typed-module.h').write_text(production.replace(before,'bit=site->moduleoffset/8;',1))
        subprocess.run(command,check=True)
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
        assert result.returncode==10 and 'MP import metadata refusal removed: case=0' in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result.stderr
        print('PASS active String MP import control:',arch,'module-root map',flush=True)
        (work/'native64-typed-module.h').unlink()
        body=extract(source,'void\nnewmp(');assert body.count('h->ref++;')==1
        file.write_text(source.replace(body,body.replace('h->ref++;',f'if(size!={clientbytes})h->ref++;'),1))
        subprocess.run(command,check=True)
        result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
        assert result.returncode==10 and 'bound module clone retain omitted' in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result.stderr
        print('PASS active String MP import control:',arch,'bound module clone retain',flush=True)

if __name__=='__main__':main()
