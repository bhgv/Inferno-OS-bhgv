#!/usr/bin/env python3
"""Checked source LOAD calls a file Command/Wm.init with real managed argv roots."""
import argparse,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FILES=runpy.run_path(str(ROOT/'utils/tests/native64-typed-files.py'))
EXT=FILES['EXT'];NS=FILES['NS'];CMD=runpy.run_path(str(ROOT/'utils/tests/dis64-command-alias.py'))
INTERFACE=CMD['CASES']['INTERFACE']
SOURCE='''implement Launch;include "provider.m";
Launch:module{open:fn(path:string);run:fn(ctxt:ref Draw->Context,argv:list of string);clear:fn();};
p:Command;calls:int=17;
open(path:string){p=load Command path;}
run(ctxt:ref Draw->Context,argv:list of string){calls++;p->init(ctxt,argv);p->init(ctxt,argv);}
clear(){p=nil;}
'''

ARGV_SOURCE='implement Launch;include "provider.m";\nLaunch:module{open:fn(path:string);run:fn(ctxt:ref Draw->Context,argv:list of string);ready:fn():int;clear:fn();};\np:Command;prepared:list of string;calls:int=17;\nopen(path:string){p=load Command path;}\nrun(ctxt:ref Draw->Context,args:list of string){calls++;if(args==nil)args="-l"::nil;else args="sh"::"-c"::"$*"::args;prepared=args;p->init(ctxt,args);p->init(ctxt,args);}\nready():int{if(p==nil)return 0;return 1;}\nclear(){prepared=nil;p=nil;}\n'

def verify_nilbranches(data,row,imports,rows=None):
    """Prove exact pointer widening/swapping, then reuse the classic relocation oracle."""
    codes=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['OP'].CODES
    classic=EXT['BASE']['READ']['legacy_instructions'](data)[1]
    normal=json.loads(json.dumps(row));maps={space:{leaf[0]+i:(leaf[1]+i,leaf[4]) for leaf in row[space]['leaves']for i in range(leaf[2])}for space in ('frame','data')}
    count=0;nil=next(g[1]for g in row['globals']if g[0]=='nil')
    for i,old in enumerate(classic[row['first']:row['first']+len(row['code'])]):
        if old['op']not in (codes['IBEQW'],codes['IBNEW']):continue
        sm=old['mode']>>3&7;mm=1 if old['mode']&192==128 else 0 if old['mode']&192==192 else -1
        if sm not in (0,1)or mm not in (0,1):continue
        a,ak=maps['frame'if sm else'data'][old['s'][0]];b,bk=maps['frame'if mm else'data'][old['m']]
        if ak not in (10,11,20)and bk not in (10,11,20):continue
        assert (ak in (10,11)and bk==20)or(ak==20 and bk in (10,11))
        left,right,mode=a,b,sm
        if ak==20:left,right,mode=b,a,mm
        assert right==nil
        for space,offset in (('frame'if mode else'data',left),('data',right)):
            assert offset%8==0 and row[space]['map'][offset//64]&(128>>(offset//8%8))
        expected=[codes['IBEQL']if old['op']==codes['IBEQW']else codes['IBNEL'],(mode<<3)|192|2,left,right,old['d'][0]-row['first'],0]
        assert row['code'][i]==expected,(row['name'],i,row['code'][i],expected)
        normal['code'][i]=[old['op'],old['mode'],a,b,old['d'][0]-row['first'],0];count+=1
    EXT['verify'](data,normal,imports,loads=True,rows=rows)
    return count

def generate(limbo,work,implementation,fault,argv=False,local=False,pair=False):
    # Keep the scalar namespace fixture's unused compatibility header independent.
    FILES['generate'](limbo,work,1)
    (work/'command.m').write_text(INTERFACE);(work/'provider.m').write_text(INTERFACE)
    bad='{z:=0;one:=1;v:=one/z;}'
    body='alias:=ctxt;ctxt=nil;keep:=alias;if(len argv!=2)'+bad+'a:=hd argv;tail:=tl argv;b:=hd tail;if(len a!=3 || len b!=2)'+bad+'local:=a::argv;argv=nil;copy:=tl local;again:=hd copy;if(len local!=3 || len copy!=2 || len again!=3)'+bad
    if argv:
        body='alias:=ctxt;ctxt=nil;keep:=alias;if(len argv!=1 && len argv!=5)'+bad+'a:=hd argv;if(len a!=2)'+bad+'if(len argv==5){xs:=tl argv;b:=hd xs;xs=tl xs;c:=hd xs;xs=tl xs;d:=hd xs;xs=tl xs;e:=hd xs;if(len b!=2 || len c!=2 || len d!=3 || len e!=2)'+bad+'}'
    provider='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'+(bad if fault else body)+'}'
    provider=CMD['CASES']['implementation'](provider,implementation)
    src=work/'wm.b';src.write_text(provider);image=work/'wm.dis64'
    result=subprocess.run([str(limbo),'-64b','-I',str(work),'-I',str(ROOT/'module'),'-o',str(image),str(src)],capture_output=True,text=True)
    assert not result.returncode,result.stderr
    # Obtain existing Command request bytes from an independent file producer.
    probe='implement Probe;include "command.m";Probe:module{init:fn();};init(){p:=load Command "library.dis64";p->init(nil,nil);}'
    src.write_text(probe);trial=work/'probe.dis64'
    result=subprocess.run([str(limbo),'-64c','-I',str(work),'-I',str(ROOT/'module'),'-o',str(trial),str(src)],capture_output=True,text=True)
    assert not result.returncode,result.stderr
    parts=CMD['SECTIONS'](trial.read_bytes());requests,names=parts[5],parts[7]
    startup_source=ARGV_SOURCE
    if argv and implementation!='Command':
        startup_source=startup_source.replace('if(args==nil)args="-l"::nil;else args="sh"::"-c"::"$*"::args;', 'if(nil!=args)args="sh"::"-c"::"$*"::args;else args="-l"::nil;')
        startup_source=startup_source.replace('if(p==nil)return 0;return 1;', 'if(nil!=p)return 1;return 0;')
    if local:
        assert argv
        prepare='if(args==nil)args="-l"::nil;else args="sh"::"-c"::"$*"::args;'if implementation=='Command'else'if(nil!=args)args="sh"::"-c"::"$*"::args;else args="-l"::nil;'
        assert prepare in startup_source
        startup_source=startup_source.replace('p=load Command path;', 'next:=resolve_forward(path);p=next;').replace(prepare,'args=prepare_forward(args);')
        startup_source+='resolve(path:string):Command{next:=load Command path;return next;}resolve_forward(path:string):Command{return resolve(path);}'
        startup_source+='prepare(args:list of string):list of string{'+prepare+'return args;}prepare_forward(args:list of string):list of string{return prepare(args);}'
    if pair:
        assert local
        startup_source=startup_source.replace('args=prepare_forward(args);prepared=args;p->init(ctxt,args);p->init(ctxt,args);',
            '(next,words):=loadmod_forward(p,args);prepared=words;next->init(ctxt,words);next->init(ctxt,words);')
        startup_source+='loadmod(next:Command,args:list of string):(Command,list of string){words:=prepare_forward(args);return (next,words);}'
        startup_source+='loadmod_forward(next:Command,args:list of string):(Command,list of string){return loadmod(next,args);}'
    data,rows=EXT['compile_source'](limbo,work,startup_source if argv else SOURCE,interface=INTERFACE,loads=True)
    assert len(rows)==(10 if pair else 8 if local else 4 if argv else 3) and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    imports=EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];assert len(imports)==1 and len(imports[0])==1;imports=imports[0]
    if argv:
        assert sum(verify_nilbranches(data,r,imports,rows=rows if local else None)for r in rows)==2
        damaged=json.loads(json.dumps(next(r for r in rows if r['name']=='ready')));damaged['code'][0][0]=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['OP'].CODES['IBEQW']
        try:verify_nilbranches(data,damaged,imports,rows=rows if local else None)
        except AssertionError:pass
        else:raise AssertionError('narrow pointer comparison escaped oracle')
    else:
        for r in rows:EXT['verify'](data,r,imports,loads=True)
    if local:
        EXT['MODULE']['check_exports'](data,rows)
        assert {r['name']for r in rows if r['signature']is None}==({'resolve','resolve_forward','loadmod','loadmod_forward'}if pair else{'resolve','resolve_forward'})
        calls=[site for r in rows for site in r.get('calls',[])];assert len(calls)==(6 if pair else 4) and sum(c[6]for c in calls)==(3 if pair else 2)
        for kind in ('target','contract','inherited','suffix'):
            badrows=json.loads(json.dumps(rows));badrow=next(r for r in badrows if r['name']=='resolve_forward')
            if kind=='target':badrow['calls'][0][2]=999999
            if kind=='contract':badrow['calls'][0][5]=8
            if kind=='inherited':badrow['calls'][0][6]=0
            if kind=='suffix':badrow['code'][-1][0]=0
            try:verify_nilbranches(data,badrow,imports,rows=badrows)
            except (AssertionError,StopIteration):pass
            else:raise AssertionError('local/import joint oracle missed '+kind)
        print('PASS local LOAD source:', 'six private helpers with pair results'if pair else'four private Module/List helpers', 'fresh/inherited contracts, unavailable private module hashes and four joint-oracle controls',flush=True)
    signature=imports[0]['signature'];assert imports[0]['name']=='init'
    assert NS['C'].BASE.Reader(requests).word()==1 and int.from_bytes(requests[16:20],'big')==0xface0186 and int.from_bytes(requests[20:24],'big')==signature
    header=subprocess.check_output([str(limbo),'-H','-M','Command','-I',str(ROOT/'module'),str(work/'command.m')],text=True)
    assert int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Command_init (0x[0-9a-f]+)U',header,re.M)[1],16)==signature
    assert '#define F_Command_init_size 80' in header and '#define F_Command_init_map {0x00,0xc0}' in header
    assert 'offsetof(F_Command_init, ctxt) != 64' in header and 'offsetof(F_Command_init, argv) != 72' in header
    lines=['#include "native64-typed-module.h"']
    def array(name,data):lines.append('static const uchar '+name+'[]={'+','.join(map(str,data))+'};')
    array('command_file_requests',requests);array('command_file_names',names)
    lines+=['static const uint command_hashes[]={'+str(signature)+'U};','static const Native64TypedImportRequest command_file_tables[]={{{command_file_requests,command_file_names,sizeof(command_file_requests),sizeof(command_file_names)},command_hashes,1}};']
    for r in rows:
        n=r['name'];lines += [f'static const Native64TypedStep command_code_{n}[]={{']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};']
        array('command_map_'+n,r['frame']['map'])
        sites=[(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])for e in r.get('externals',[])]
        sites += [(a,b,next(i for i,row in enumerate(rows)if row['first']==target),contract,inherited,0,0,0,0)for a,b,target,desc,offset,contract,inherited in r.get('calls',[])]
        if sites:lines += [f'static const Native64ModuleSite command_sites_{n}[]={{']+['{'+','.join(map(str,e))+'},'for e in sorted(sites)]+['};']
    sites=next(r for r in rows if r['name']=='run')['externals'];assert len(sites)==2 and all([a[0]for a in s['args']]==[64,72]and s['contract']==0 for s in sites)
    array('command_request_map',[0,192]);lines += ['static const Native64ModuleImport command_requests[]={{"init",'+str(signature)+'U,80,command_request_map,2}};','static const Native64ModuleTable command_tables[]={{command_requests,1}};','static const Native64ModuleFunction command_functions[]={']
    exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1];exports={e['name']:e for e in exports}
    for r in rows:
        n=r['name'];e=exports.get(n);assert e is None or e['pc']==r['first'];sites=r.get('externals',[])+r.get('calls',[])
        lines.append('{'+json.dumps(n)+f',{e["signature"]if e else 0}U,{int(e is not None)},command_code_{n},{len(r["code"])},{r["frame"]["bytes"]},command_map_{n},{len(r["frame"]["map"])},'+(f'command_sites_{n}'if sites else'nil')+f',{len(sites)},nil,0,{48 if r.get("result_pair")else 0}'+'},')
    lines+=['};'];r=rows[0];g={v[0]:v[1]for v in r['globals']};assert {'p','nil','calls'}<=set(g) if argv else set(g)=={'p','nil','calls'}
    literals=[]
    if argv:
        initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3]
        for old,cell in sorted(initial.items()):
            if cell['kind']!='string':continue
            leaf=next(l for l in r['data']['leaves']if l[0]==old);assert leaf[2:]==[4,8,13]
            text=bytes(cell['bytes']);array('command_literal_'+str(old),text);literals.append((leaf[1],text,'command_literal_'+str(old)))
        assert {text for _,text,_ in literals}=={b'-l',b'sh',b'-c',b'$*'}
        lines+=['static const Native64ModuleString command_strings[]={']+['{'+f'{offset},{len(text)},{name}'+'},'for offset,text,name in literals]+['};']
    array('command_data_map',r['data']['map']);lines += ['static const Native64ModuleInit command_initial[]={{'+str(g['calls'])+',4,17}};','static const Native64ModuleSpec command_spec={"Launch","trusted/Launch",command_functions,'+str(len(rows))+',~0U,'+f'{r["data"]["bytes"]},command_data_map,{len(r["data"]["map"])},command_initial,1,command_tables,1,'+('c2string,command_strings,'+str(len(literals))if argv else'nil,nil,0')+',read64ownedimport'+'};']
    for i,r in enumerate(rows):lines.append(f'static const uint command_{r["name"]}={i};')
    if not argv:lines+=['static const uint command_ready=~0U;']
    if not local:lines+=['static const uint command_resolve=~0U,command_prepare=~0U;']
    if not pair:lines+=['static const uint command_loadmod=~0U;']
    lines += [f'static const uint command_pair={int(pair)},command_local={int(local)};']
    lines += [f'static const uint command_argv={int(argv)},command_prepared_offset={g.get("prepared",0)};']
    if argv:
        opcodes=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['OP'].CODES
        publication=[i for i,x in enumerate(next(row for row in rows if row['name']=='run')['code'])if x[0]==opcodes['IMOVP']and x[1]&7==0 and x[4]==g['prepared']]
        assert len(publication)==1
        lines += ['static const uint command_publish_index='+str(publication[0])+';']
    if argv:lines+=['static const uint command_literal_offsets[]={'+','.join(str(offset)for offset,_,_ in literals)+'};','static const char *command_literal_text[]={'+','.join(json.dumps(text.decode())for _,text,_ in literals)+'};']
    else:lines+=['static const uint command_literal_offsets[]={0};','static const char *command_literal_text[]={""};']
    if pair:
        body=next(row for row in rows if row['name']=='loadmod');offset=sum(len(row['code'])for row in rows[:rows.index(body)])
        stores=[(i,step[5])for i,step in enumerate(body['code'])if step[5]in(40,56)];assert [v for _,v in stores]==[40,56]
        lines+=['static const uint command_pair_store0='+str(offset+stores[0][0])+',command_pair_store1='+str(offset+stores[1][0])+';']
        run=next(row for row in rows if row['name']=='run');sites=[site for site in run['calls']if site[5]==48];assert len(sites)==1 and not sites[0][6]
        lines+=['static const uint command_pair_offset='+str(sites[0][4])+';']
    else:lines+=['static const uint command_pair_store0=~0U,command_pair_store1=~0U,command_pair_offset=0;']
    lines += [f'static const uint command_provider_offset={g["p"]},command_calls_offset={g["calls"]},command_fault={int(fault)};']
    (work/'typed-command-files.h').write_text('\n'.join(lines)+'\n')
    print('PASS Command file source:',implementation,'fault'if fault else'consume',str(len(rows))+' checked bodies, independent existing Command bytes/.m hash and mapped 80-byte import frame',flush=True)
    return image

def unit():
    source=FILES['unit']();source=source.replace('int main(int argc,char **argv)','int scalarfilemain(int argc,char **argv)',1)
    source=source.replace('#include "tests/leaf-cons-stub.h"','#define cons unused_command_cons\n#define Tptr unused_command_Tptr\n#include "tests/leaf-cons-stub.h"\n#undef cons\n#undef Tptr',1)
    extract=NS['S']['FILES']['FILES']['function'];heap=(ROOT/'libinterp/heap.c').read_text();xec=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
    source=source.replace(extract(source,'String*\nnewrunes('),extract(strings,'String*\nnewrunes('),1)
    source+='\nvoid freelist(Heap*,int);\nType Tlist={1,freelist,markheap,sizeof(List)};\nType Tptr={1,0,markheap,sizeof(void*),1,0,0,{0x80}};\n'
    source+=extract(heap,'void\nfreelist(')+'\n#define movp actual_movp\n'
    for sig in ('List*\ncons(','static List*\nlisthead(','OP(consp)','OP(headp)','OP(tail)','OP(lenl)'):source+=extract(xec,sig)+'\n'
    source+='\n#undef movp\n'+extract(strings,'OP(lenc)')+'\n'
    source=source.replace(extract(source,'String*\nc2string('),extract(strings,'String*\nc2string('),1)
    source+='\n#undef T\n'+'\n'.join(line for line in (ROOT/'lib9/rune.c').read_text().splitlines()if not line.startswith('#include'))+'\n#undef T\n'
    start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start);handlers=source[start:end]
    return source+'\n#line 1 "native64-typed-command-files.c"\n'+(ROOT/'utils/tests/native64-typed-command-files.c').read_text().replace(' COMMAND_HANDLERS',handlers)

def main(argv=False,local=False,pair=False):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
    if not __debug__:p.error('source/file oracles require assertions')
    if pair:runpy.run_path(str(ROOT/'utils/tests/dis64-typed-pairs.py'))['main']()
    if local:runpy.run_path(str(ROOT/'utils/tests/dis64-typed-signature.py'))['main']()
    CMD['witnesses']()
    if argv:
        text=(ROOT/'appl/cmd/emuinit.b').read_text()
        assert 'args = "-l" :: nil;'in text and 'return (mod, "sh" :: "-c" :: "$*" :: args);'in text and 'mod->init(nil, args);'in text
        print('PASS actual emuinit argv and Limbo init source witnesses',flush=True)
    with tempfile.TemporaryDirectory(prefix='typed-command-files-',dir=ROOT)as td:
        work=Path(td);src=work/'test.c';source=unit();src.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(work)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'test';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+FILES['releaseflags'](cc)+['-lm','-o',str(binary)]
        if argv:
            bad='implement Bad;R:adt{n:int;};P:module{f:fn();};Bad:module{nested:fn(x:list of list of string):int;arr:fn(x:array of byte):int;refnil:fn(x:ref R):int;mods:fn(a:P,b:P):int;};nested(x:list of list of string):int{if(x==nil)return 1;return 0;}arr(x:array of byte):int{if(x==nil)return 1;return 0;}refnil(x:ref R):int{if(x==nil)return 1;return 0;}mods(a:P,b:P):int{if(a==b)return 1;return 0;}'
            _,refused=EXT['compile_source'](a.limbo.resolve(),work,bad,interface='',loads=True)
            assert len(refused)==4 and all(not row['accepted']and row['reason']=='load-nil-roles'for row in refused)
            _,old=EXT['compile_source'](a.limbo.resolve(),work,ARGV_SOURCE,interface=INTERFACE,loads=False)
            assert all(not row['accepted']for row in old if row['name']in('run','ready'))
            print('PASS startup nil boundary: four nested/array/ref/module-pair refusals, external-only mode remains closed',flush=True)
        if local:
            pair_source='implement Pair;include "provider.m";Pair:module{pair:fn(p:Command,args:list of string):(Command,list of string);};pair(p:Command,args:list of string):(Command,list of string){return (p,args);}'
            _,refused=EXT['compile_source'](a.limbo.resolve(),work,pair_source,interface=INTERFACE,loads=False)
            assert len(refused)==1 and not refused[0]['accepted']and refused[0]['reason']=='aggregate-result'
            print('PASS external-only tuple boundary: module/list aggregate result remains refused',flush=True)
        if pair:
            badpair='implement Bad;include "provider.m";Bad:module{reversed:fn(p:Command,a:list of string):(list of string,Command);ints:fn(p:Command,a:list of int):(Command,list of int);three:fn(p:Command,a:list of string):(Command,list of string,int);scalar:fn(p:Command,a:int):(Command,int);};reversed(p:Command,a:list of string):(list of string,Command){return(a,p);}ints(p:Command,a:list of int):(Command,list of int){return(p,a);}three(p:Command,a:list of string):(Command,list of string,int){return(p,a,1);}scalar(p:Command,a:int):(Command,int){return(p,a);}'
            _,refused=EXT['compile_source'](a.limbo.resolve(),work,badpair,interface=INTERFACE,loads=True)
            assert len(refused)==4 and all(not r['accepted']and r['reason']=='aggregate-result'for r in refused)
            print('PASS pair source boundary: reversed/list-int/three-field/scalar aggregates refused',flush=True)
        for implementation,fault in (('Command',False),('Wm',False),('Wm',True)):
            image=generate(a.limbo.resolve(),work,implementation,fault,argv=argv,local=local,pair=pair);subprocess.run(command,check=True)
            r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(image)],capture_output=True,text=True,timeout=180)
            assert not r.returncode and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
            for line in r.stdout.splitlines():
                if line.startswith('PASS '):print(line,flush=True)
            if implementation=='Command':
                controls=[('if(dis64commandsignature(&binding.signature) && hash!=dis64u32(binding.signature.data+4))return 0;',';','Command install guard omitted: case=8'),
                          ('if(type==nil || type==H || type->ref<=0 || type->size!=80 || type->np!=2 || type->map[0] || type->map[1]!=0xc0)return 0;','if(type==nil || type==H || type->ref<=0 || type->size!=80 || type->np!=2 || type->map[0])return 0;','Command install guard omitted: case=1')]
                runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
                for before,after,diagnostic in controls:
                    assert source.count(before)==1
                    src.write_text(source.replace(before,after,1));subprocess.run(command,check=True)
                    r=subprocess.run(runner+[str(binary),str(image),'install-only'],capture_output=True,text=True,timeout=180)
                    assert r.returncode==10 and diagnostic in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                    print('PASS Command active control:',arch,diagnostic,flush=True)
                extract=NS['S']['FILES']['FILES']['function'];body=extract(source,'OP(consp)');assert body.count('h->ref++;')==1
                src.write_text(source.replace(body,body.replace('h->ref++;',';'),1));subprocess.run(command,check=True)
                r=subprocess.run(runner+[str(binary),str(image)],capture_output=True,text=True,timeout=180)
                assert r.returncode==10 and 'Command file predicate: argv head retain omitted'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                print('PASS Command active control:',arch,'argv head retain',flush=True)
                src.write_text(source)
                if argv:
                    header=work/'typed-command-files.h';original=header.read_text();match=re.search(r'static const Native64TypedStep command_code_run\[\]=\{(.*?)\};',original,re.S);assert match
                    rows=match[1].strip().splitlines();index=int(re.search(r'command_publish_index=(\d+);',original)[1]);step=list(map(int,rows[index].strip('{},').split(',')));assert step[1]>>3&7==1 and step[1]&7==0
                    step[1]=(step[1]&~7)|1;step[4]=step[2];rows[index]='{'+','.join(map(str,step))+'},'
                    header.write_text(original[:match.start(1)]+'\n'+'\n'.join(rows)+'\n'+original[match.end(1):]);subprocess.run(command,check=True)
                    r=subprocess.run(runner+[str(binary),str(image)],capture_output=True,text=True,timeout=180)
                    assert r.returncode==10 and 'Command file predicate: argv MP publication'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                    print('PASS startup active control:',arch,'prepared argv MP publication',flush=True);header.write_text(original)
                if local:
                    materializer=(ROOT/'include/native64-typed-multi.h').read_text();old='p->d.i.s=s->d;';assert materializer.count(old)==1
                    (work/'native64-typed-module.h').write_text((ROOT/'include/native64-typed-module.h').read_text())
                    (work/'native64-typed-multi.h').write_text(materializer.replace(old,'p->d.i.s=(c->inherited && j==c->end-1)?48:s->d;',1))
                    subprocess.run(command,check=True)
                    r=subprocess.run(runner+[str(binary),str(image)],capture_output=True,text=True,timeout=180)
                    assert r.returncode==10 and 'Command file predicate: inherited result address publication'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                    print('PASS private call active control:',arch,'inherited result address publication',flush=True)
                    (work/'native64-typed-module.h').unlink();(work/'native64-typed-multi.h').unlink()
                if pair:
                    module=(ROOT/'include/native64-typed-module.h').read_text();old='at<(unsigned long)step->s+16';assert module.count(old)==1
                    (work/'native64-typed-module.h').write_text(module.replace(old,'at<(unsigned long)step->s+8',1))
                    subprocess.run(command,check=True)
                    r=subprocess.run(runner+[str(binary),str(image),'install-only'],capture_output=True,text=True,timeout=180)
                    assert r.returncode==10 and 'Command file predicate: pair preflight refusal'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                    print('PASS pair active control: constructor checks second root',flush=True)
                    (work/'native64-typed-module.h').unlink()
                    bind=(ROOT/'include/native64-local-bind.h').read_text();old='at<(unsigned int)instruction.s.ind+width';assert bind.count(old)==1
                    (work/'native64-local-bind.h').write_text(bind.replace(old,'at<(unsigned int)instruction.s.ind+(contract==48?8:width)',1))
                    subprocess.run(command,check=True)
                    r=subprocess.run(runner+[str(binary),str(image),'pair-root-control'],capture_output=True,text=True,timeout=180)
                    assert r.returncode==10 and 'Command file predicate: pair result root map'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                    print('PASS pair active control: runtime binder checks second root before writes',flush=True)
                    (work/'native64-local-bind.h').unlink()


if __name__=='__main__':main()
