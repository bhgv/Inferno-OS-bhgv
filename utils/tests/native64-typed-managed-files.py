#!/usr/bin/env python3
"""Checked-IR LOAD calls the existing String/List file trial via an owned import table."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'))
EXT=CMD['EXT'];NS=CMD['NS'];MAN=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-module.py'))

def generate(limbo,work,module,pack,unpack,reverse):
    CMD['generate'](limbo,work,'Command',False)
    interface=MAN['interface'](limbo,work,module,pack,unpack)
    (work/'provider.m').write_text(interface)
    canonical=subprocess.check_output([str(limbo),'-H','-M','String',str(ROOT/'module/string.m')],text=True)
    (work/'string-native64.h').write_text(canonical)
    image=work/'managed.dis64'
    image.write_bytes(MAN['compile_file'](limbo,work,MAN['provider_source'](module,pack,unpack,reverse),'-64m',image))
    probe='implement Probe;include "named.m";Probe:module{init:fn();};init(){p:=load '+module+' "/library.dis64";xs:=p->'+pack+'("probe");s:=p->'+unpack+'(xs);}'
    trial=MAN['compile_file'](limbo,work,probe,'-64l',work/'probe.dis64')
    parts=CMD['CMD']['SECTIONS'](trial);requests,names=parts[5],parts[7]
    source='implement Use;include "provider.m";Use:module{open:fn(path:string);round:fn(s:string):string;split:fn(s:string):list of string;join:fn(xs:list of string):string;clear:fn();};p:'+module+';calls:int=17;open(path:string){p=load '+module+' path;}round(s:string):string{calls++;xs:=p->'+pack+'(s);return p->'+unpack+'(xs);}split(s:string):list of string{calls++;return p->'+pack+'(s);}join(xs:list of string):string{calls++;return p->'+unpack+'(xs);}clear(){p=nil;}'
    data,rows=EXT['compile_source'](limbo,work,source,interface=interface,loads=True)
    assert len(rows)==5 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    imports=EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];assert len(imports)==1;imports=imports[0]
    for r in rows:EXT['verify'](data,r,imports,loads=True)
    symbols=MAN['import_symbols'](trial,module,pack,unpack);assert len(imports)==2
    records=[requests[4+i*32:4+(i+1)*32]for i in range(2)];assert len(requests)==68
    requests=NS['C'].words(2)+b''.join(records[symbols[e['name']]]for e in imports)
    lines=['#include "native64-typed-module.h"']
    def array(n,b):lines.append('static const uchar '+n+'[]={'+','.join(map(str,b))+'};')
    array('managed_requests',requests);array('managed_names',names)
    lines+=['static const uint managed_hashes[]={'+','.join(str(e['signature'])+'U'for e in imports)+'};','static const Native64TypedImportRequest managed_tables[]={{{managed_requests,managed_names,sizeof(managed_requests),sizeof(managed_names)},managed_hashes,2,1}};']
    for r in rows:
        n=r['name'];lines += [f'static const Native64TypedStep managed_code_{n}[]={{']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};'];array('managed_map_'+n,r['frame']['map'])
        sites=r.get('externals',[])
        if sites:lines += [f'static const Native64ModuleSite managed_sites_{n}[]={{']+['{'+','.join(map(str,(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])))+'},'for e in sites]+['};']
    array('managed_request_map',[0,128]);lines+=['static const Native64ModuleImport managed_imports[]={']+['{'+json.dumps(e['name'])+f',{e["signature"]}U,72,managed_request_map,2'+'},'for e in imports]+['};','static const Native64ModuleTable managed_mt[]={{managed_imports,2}};']
    exports={e['name']:e for e in EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1]}
    lines+=['static const Native64ModuleFunction managed_functions[]={']
    for r in rows:
        n=r['name'];e=exports[n];sites=r.get('externals',[]);assert e['pc']==r['first']
        lines.append('{'+json.dumps(n)+f',{e["signature"]}U,1,managed_code_{n},{len(r["code"])},{r["frame"]["bytes"]},managed_map_{n},{len(r["frame"]["map"])},'+(f'managed_sites_{n}'if sites else'nil')+f',{len(sites)}'+'},')
    lines+=['};'];r=rows[0];g={a[0]:a[1]for a in r['globals']};assert set(g)=={'p','nil','calls'}
    array('managed_data_map',r['data']['map']);lines+=['static const Native64ModuleInit managed_initial[]={{'+str(g['calls'])+',4,17}};','static const Native64ModuleSpec managed_spec={"Use","trusted/Use",managed_functions,5,~0U,'+f'{r["data"]["bytes"]},managed_data_map,{len(r["data"]["map"])},managed_initial,1,managed_mt,1,nil,nil,0,read64ownedimport'+'};']
    for i,r in enumerate(rows):lines.append(f'static const uint managed_{r["name"]}={i};')
    lines += [f'static const uint managed_provider_offset={g["p"]},managed_calls_offset={g["calls"]};']
    (work/'typed-managed-files.h').write_text('\n'.join(lines)+'\n')
    print('PASS managed checked source:',module,pack,unpack,'independent trial records, classic hashes, 72-byte mapped frames, five checked bodies',flush=True)
    return image

def unit():
    source='static int dis64bootmanaged; static char *dis64bootprovider;\n'+CMD['unit']().replace('int main(int argc,char **argv)','int commandfilemain(int argc,char **argv)',1)
    load=(ROOT/'libinterp/load64.c').read_text();extract=NS['S']['FILES']['FILES']['function']
    source=source.replace('#include "fns.h"','#include "fns.h"\nModule *prepare64managedfile(char*,uchar*,ulong,int);',1)
    source+='\n#include "dis64-managed-signature.h"\n#include "dis64-storage-image.h"\n'
    source+=load[load.index('struct Dis64StorageOwner {'):load.index('typedef struct Dis64LibraryBuild')]
    source+=load[load.index('/* Consume a staging lease'):load.index('typedef struct Dis64StorageClone')]
    source+=extract(load,'int\nattach64storage(')+'\n'
    source+=load[load.index('typedef struct Dis64NumericLoad'):load.index('#ifdef DIS_NATIVE64_BOOT_SYS_IO',load.index('typedef struct Dis64NumericLoad'))]
    source+=extract(load,'static int\ndis64numericconstruct(')+'\n'
    source+=load[load.index('static const Dis64StorageLimits dis64numericlimits'):load.index('#ifdef DIS_NATIVE64_BOOT_SYS_IO',load.index('static const Dis64StorageLimits dis64numericlimits'))]
    source+=load[load.index('typedef struct Dis64ManagedFileCatalog'):load.index('/* The ordinary parser owns signing, namespace metadata and cache publication. */')]
    source+=extract(load,'Module*\nprepare64managedfile(')+'\n'
    source=source.replace('Modlink*\nprepare64managedlibrarylink(','Modlink*\nactualprepare64managedlibrarylink(',1)
    source+='\nModlink*prepare64managedlibrarylink(char*p,uchar*i,ulong b,int j,const uchar*r,ulong rb,const uchar*s,ulong sb){require(!fileopen && worker.iprog==nil);namespaceconstructors++;return actualprepare64managedlibrarylink(p,i,b,j,r,rb,s,sb);}\n'
    start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start)
    return source+'\n#line 1 "native64-typed-managed-files.c"\n'+(ROOT/'utils/tests/native64-typed-managed-files.c').read_text().replace(' MANAGED_HANDLERS',source[start:end])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
    if not __debug__:p.error('independent source/file oracles require assertions')
    with tempfile.TemporaryDirectory(prefix='typed-managed-files-',dir=ROOT)as td:
        w=Path(td);src=w/'test.c';source=unit();src.write_text(source)
        arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_NATIVE64_MANAGED_FILES=1','-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(w/'test')]
        for args in [('Box','pack','unpack',False),('Other','zpack','ahead',True)]:
            image=generate(a.limbo.resolve(),w,*args);subprocess.run(command,check=True)
            r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'test'),str(image)],capture_output=True,text=True,timeout=180)
            assert not r.returncode and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
            for line in r.stdout.splitlines():
                if line.startswith('PASS '):print(line,flush=True)
            if args[0]=='Box':
                runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
                r=subprocess.run(runner+[str(w/'test'),str(image),'default'],capture_output=True,text=True,timeout=90)
                assert not r.returncode,(r.returncode,r.stdout,r.stderr)
                print(r.stdout.strip(),flush=True)
                controls=[('if(hash!=canonical)return 0;',';','managed install guard omitted: case=7',True),
                          ('type->map[1]!=0x80)return 0;','0)return 0;','managed install guard omitted: case=5',True),
                          ('owner->managed=managedtables;',';','managed file predicate: owned native managed selector',False)]
                for old,new,diagnostic,installonly in controls:
                    # Restrict map mutation to installer; other proofs stay intact.
                    extract=NS['S']['FILES']['FILES']['function']
                    body=extract(source,'int\ninstall64typedimports(') if installonly else source
                    assert body.count(old)==1
                    src.write_text(source.replace(body,body.replace(old,new,1),1));subprocess.run(command,check=True)
                    r=subprocess.run(runner+[str(w/'test'),str(image)]+(['install-only']if installonly else[]),capture_output=True,text=True,timeout=180)
                    assert r.returncode==10 and diagnostic in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                    print('PASS managed active control:',arch,diagnostic,flush=True)
                src.write_text(source)

        # Disabled entry points refuse hostile pointer sentinels without inspection.
        extract=NS['S']['FILES']['FILES']['function'];read=(ROOT/'libinterp/readmod.c').read_text();load=(ROOT/'libinterp/load64.c').read_text()
        disabled=w/'disabled.c'
        disabled.write_text('#include "lib9.h"\n#include "isa.h"\n#include "interp.h"\nvoid kwerrstr(char *s,...){USED(s); }\n'+extract(read,'static Modlink*\nread64librarymode(')+'\n'+extract(read,'Modlink*\nread64managedlibrary(')+'\n'+extract(load[load.rindex('#if !defined(DIS_NATIVE64_MANAGED_FILES)'):],'Modlink*\nprepare64managedlibrarylink(')+'\nint main(void){return read64managedlibrary((char*)H,2,(uchar*)H,~0U,(uchar*)H,~0U,2)!=nil || prepare64managedlibrarylink((char*)H,(uchar*)H,~0U,2,(uchar*)H,~0U,(uchar*)H,~0U)!=nil;}\n')
        subprocess.run(cc+['-UDIS64_PILOT_LOADER','-DDIS64_PILOT_LOADER=0',str(disabled),'-o',str(w/'disabled')],check=True)
        subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'disabled')],check=True,timeout=90)
        print('PASS managed disabled service:',arch,'no sentinel dereference',flush=True)
if __name__=='__main__':main()
