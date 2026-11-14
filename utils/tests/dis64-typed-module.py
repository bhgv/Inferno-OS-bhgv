#!/usr/bin/env python3
"""Match compiler signatures/exports to classic Dis and emit a trusted module spec."""
from pathlib import Path
import json
import runpy
ROOT=Path(__file__).resolve().parents[2]
MULTI=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-multi.py'))
SOURCE=MULTI['SOURCE'].replace('leaf_void(n:int){out=n;}','leaf_void(n:int){v:=hidden(n);out=v;}')+'hidden(n:int):int{return n;}\n'

def check_exports(data,rows):
    header,code,types,initial,module,exports=MULTI['BASE']['READ']['legacy_instructions'](data,with_exports=True)
    byname={x['name']:x for x in exports};assert len(byname)==len(exports)
    for row in rows:
        assert row['accepted']and 0<=row['signature']<=0xffffffff
        e=byname.get(row['export_name'])
        if e:assert (e['pc'],e['descriptor'],e['signature'])==(row['first'],row['descriptor'],row['signature'])
    assert set(byname)=={r['export_name']for r in rows if r['export_name']in byname}
    return module,byname

def generate(limbo,work):
    data,rows=MULTI['BASE']['compile_source'](limbo,work,SOURCE,multi=True)
    assert len(rows)==24 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    for r in rows:MULTI['verify'](data,r,rows)
    module,exports=check_exports(data,rows);assert len(exports)==23 and 'hidden'not in exports
    # Compile a separate consumer against a .m interface; use its import hashes.
    interface=SOURCE[SOURCE.index('Multi:module'):SOURCE.index('};')+2]
    (work/'provider.m').write_text(interface+'\n')
    args={'count':'n','identity':'x','wide':'b','fraction':'r','octet':'y','items':'l'}
    calls=''.join(f'p->{prefix}_{name}({args[name]});'for name,_,_,_ in MULTI['KINDS']for prefix in ('relay','chain'))+'p->relay_void(n);p->chain_void(n);p->relay_forward(n);p->chain_forward(n);'
    client='implement Client;include "provider.m";Client:module{run:fn(n:int,x:string,b:big,r:real,y:byte,l:list of string);};run(n:int,x:string,b:big,r:real,y:byte,l:list of string){p:=load Multi "trusted/Multi";'+calls+'}'
    cfile=work/'client.b';cfile.write_text(client);cdis=work/'client.dis'
    MULTI['BASE']['READ']['checked_run']([str(limbo),'-i','-I',str(work),'-o',str(cdis),str(cfile)],check=True,capture_output=True)
    tables=MULTI['BASE']['READ']['legacy_instructions'](cdis.read_bytes(),with_imports=True)[-1]
    imports={e['name']:e['signature']for table in tables for e in table}
    assert len(imports)==16 and all(exports[name]['signature']==value for name,value in imports.items()),imports
    first={r['first']:i for i,r in enumerate(rows)};names={r['name']:i for i,r in enumerate(rows)}
    lines=['#include "native64-typed-module.h"']
    for r in rows:
        name=r['name'];lines+=['static const Native64TypedStep code_'+name+'[]={']+['{'+','.join(map(str,x))+'},'for x in r['code']]+['};','static const uchar map_'+name+'[]={'+','.join(map(str,r['frame']['map']))+'};','static const Native64ModuleSite sites_'+name+'[]={']
        lines+=['{'+','.join(map(str,(a,b,first[t],c,h)))+'},'for a,b,t,d,o,c,h in r.get('calls',[])]+(['{0},']if not r.get('calls')else [])+['};']
    lines+=['static const Native64ModuleFunction module_functions[]={']
    for r in rows:
        name=r['name'];lines+=['{'+json.dumps(r['export_name'])+','+str(r['signature'])+'U,'+str(int(r['export_name']in exports))+',code_'+name+','+str(len(r['code']))+','+str(r['frame']['bytes'])+',map_'+name+','+str(len(r['frame']['map']))+',sites_'+name+','+str(len(r.get('calls',[])))+'},']
    lines+=['};','static const int module_entries[8][2]={']+['{'+str(names['relay_'+name])+','+str(names['chain_'+name])+'},'for name in [x[0]for x in MULTI['KINDS']]+['void','forward']]+['};']
    header=MULTI['BASE']['READ']['legacy_instructions'](data)[0]
    entry='~0U'if header[7]<0 else str(first[header[7]])
    d=rows[0]['data'];out=next(x[1]for x in rows[0]['globals']if x[0]=='out')
    lines+=['static const unsigned int module_import_signatures[]={'+','.join(str(imports.get(r['export_name'],0))+'U'for r in rows)+'};']
    lines+=['static const uchar module_data_map[]={'+','.join(map(str,d['map']))+'};','static const Native64ModuleInit module_initial[]={{'+str(out)+',4,1234}};','static const Native64ModuleSpec module_spec={"Multi","trusted/Multi",module_functions,'+str(len(rows))+','+entry+','+str(d['bytes'])+',module_data_map,'+str(len(d['map']))+',module_initial,1};','static const unsigned int module_out='+str(out)+';']
    (work/'typed-module-native64.h').write_text('\n'.join(lines)+'\n')
    altered=json.loads(json.dumps(rows));altered[0]['signature']^=1
    try:check_exports(data,altered)
    except AssertionError:pass
    else:raise AssertionError('export hash corruption')
    methods='implement Methods;Methods:module{Box:adt{n:int;identity:fn(x:int):int;};};Box.identity(x:int):int{return x;}'
    methoddata,methodrows=MULTI['BASE']['compile_source'](limbo,work,methods,multi=True)
    _,methodexports=check_exports(methoddata,methodrows)
    assert len(methodrows)==1 and methodrows[0]['export_name']=='Box.identity'and 'Box.identity'in methodexports
    print('PASS whole module source: 24 bodies, 23 exact classic exports/signatures, 16 independent .m consumer imports, private helper excluded, qualified ADT method names and signature corruption refused',flush=True)
