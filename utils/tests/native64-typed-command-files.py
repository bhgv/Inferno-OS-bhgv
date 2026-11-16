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

def generate(limbo,work,implementation,fault):
    # Keep the scalar namespace fixture's unused compatibility header independent.
    FILES['generate'](limbo,work,1)
    (work/'command.m').write_text(INTERFACE);(work/'provider.m').write_text(INTERFACE)
    bad='{z:=0;one:=1;v:=one/z;}'
    body='alias:=ctxt;ctxt=nil;keep:=alias;if(len argv!=2)'+bad+'a:=hd argv;tail:=tl argv;b:=hd tail;if(len a!=3 || len b!=2)'+bad+'local:=a::argv;argv=nil;copy:=tl local;again:=hd copy;if(len local!=3 || len copy!=2 || len again!=3)'+bad
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
    data,rows=EXT['compile_source'](limbo,work,SOURCE,interface=INTERFACE,loads=True)
    assert len(rows)==3 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    imports=EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];assert len(imports)==1 and len(imports[0])==1;imports=imports[0]
    for r in rows:EXT['verify'](data,r,imports,loads=True)
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
        if r.get('externals'):lines += [f'static const Native64ModuleSite command_sites_{n}[]={{']+['{'+','.join(map(str,(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])))+'},'for e in r['externals']]+['};']
    sites=next(r for r in rows if r['name']=='run')['externals'];assert len(sites)==2 and all([a[0]for a in s['args']]==[64,72]and s['contract']==0 for s in sites)
    array('command_request_map',[0,192]);lines += ['static const Native64ModuleImport command_requests[]={{"init",'+str(signature)+'U,80,command_request_map,2}};','static const Native64ModuleTable command_tables[]={{command_requests,1}};','static const Native64ModuleFunction command_functions[]={']
    exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1];exports={e['name']:e for e in exports}
    for r in rows:
        n=r['name'];e=exports[n];assert e['pc']==r['first'];sites=r.get('externals',[])
        lines.append('{'+json.dumps(n)+f',{e["signature"]}U,1,command_code_{n},{len(r["code"])},{r["frame"]["bytes"]},command_map_{n},{len(r["frame"]["map"])},'+(f'command_sites_{n}'if sites else'nil')+f',{len(sites)}'+'},')
    lines+=['};'];r=rows[0];g={v[0]:v[1]for v in r['globals']};assert set(g)=={'p','nil','calls'}
    array('command_data_map',r['data']['map']);lines += ['static const Native64ModuleInit command_initial[]={{'+str(g['calls'])+',4,17}};','static const Native64ModuleSpec command_spec={"Launch","trusted/Launch",command_functions,3,~0U,'+f'{r["data"]["bytes"]},command_data_map,{len(r["data"]["map"])},command_initial,1,command_tables,1,nil,nil,0,read64ownedimport'+'};']
    for i,r in enumerate(rows):lines.append(f'static const uint command_{r["name"]}={i};')
    lines += [f'static const uint command_provider_offset={g["p"]},command_calls_offset={g["calls"]},command_fault={int(fault)};']
    (work/'typed-command-files.h').write_text('\n'.join(lines)+'\n')
    print('PASS Command file source:',implementation,'fault'if fault else'consume','three checked bodies, independent existing Command bytes/.m hash and mapped 80-byte import frame',flush=True)
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
    start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start);handlers=source[start:end]
    return source+'\n#line 1 "native64-typed-command-files.c"\n'+(ROOT/'utils/tests/native64-typed-command-files.c').read_text().replace(' COMMAND_HANDLERS',handlers)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
    if not __debug__:p.error('source/file oracles require assertions')
    CMD['witnesses']()
    with tempfile.TemporaryDirectory(prefix='typed-command-files-',dir=ROOT)as td:
        work=Path(td);src=work/'test.c';source=unit();src.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(work)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'test';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+FILES['releaseflags'](cc)+['-lm','-o',str(binary)]
        for implementation,fault in (('Command',False),('Wm',False),('Wm',True)):
            image=generate(a.limbo.resolve(),work,implementation,fault);subprocess.run(command,check=True)
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
if __name__=='__main__':main()
