#!/usr/bin/env python3
"""Run checked source LOAD through owned .m installation and real file admission."""
import argparse,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
NS=runpy.run_path(str(ROOT/'utils/tests/dis64-library-namespace.py'))
EXT=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-external.py'))

def releaseflags(cc):
    # Bionic's Inferno objects use the strict free_ seam. Static libc's FILE
    # buffers call libc free too and must not enter the VM allocation ledger.
    macros=subprocess.run(cc+['-dM','-E','-include','sys/cdefs.h','-x','c','-'],input=b'',capture_output=True,check=True).stdout
    return ['-Wl,--defsym=__real_free=free'] if re.search(rb'^#define __BIONIC__\b',macros,re.M) else ['-Wl,--wrap=free']

def generate(limbo,work,kind):
    scalar={1:'int',2:'big',3:'real'}[kind]
    interface=f'Library:module{{mid:fn(n:{scalar}):{scalar};leaf:fn(n:{scalar}):{scalar};init:fn();}};'
    leaf='n'if kind==3 else'n+n'
    provider='implement Library;'+interface+f'mid(n:{scalar}):{scalar}{{return leaf(n);}}leaf(n:{scalar}):{scalar}{{return {leaf};}}init(){{one:=1;zero:=0;v:=one/zero;}}'
    src=work/'provider.b';src.write_text(provider);image=work/'provider.dis64';env=dict(os.environ)
    subprocess.run([str(limbo),'-64b','-o',str(image),str(src)],check=True,capture_output=True,env=env)
    m=work/'provider.m';m.write_text(interface)
    result=subprocess.run([str(limbo),'-H','-R','-M','Library',str(m)],check=True,capture_output=True,text=True,env=env)
    (work/'provider.h').write_text(result.stdout);structural=NS['arrays'](result.stdout);records=NS['bindings'](structural)
    source='implement Client;include "provider.m";Client:module{open:fn(path:string);'+f'run:fn(n:{scalar}):{scalar};again:fn(n:{scalar}):{scalar};clear:fn();}};'+f'p:Library;calls:int=17;open(path:string){{p=load Library path;}}run(n:{scalar}):{scalar}{{calls++;a:=p->mid(n);return p->leaf(a);}}again(n:{scalar}):{scalar}{{return p->mid(n);}}clear(){{p=nil;}}'
    data,rows=EXT['compile_source'](limbo,work,source,interface=interface,loads=True)
    assert len(rows)==4 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    tables=EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];assert len(tables)==1;imports=tables[0]
    assert {r['name']for r in imports}=={'mid','leaf'}
    for r in rows:EXT['verify'](data,r,imports,loads=True)
    # Reorder complete structural records to exactly the source ldt ordinal order.
    reader=NS['C'].BASE.Reader(structural['imports']);count=reader.word();raw=[]
    for i in range(count):
        module,symbol,length=reader.word(),reader.word(),reader.word();sig=reader.take(length)
        raw.append(NS['C'].words(module,symbol,length)+sig)
    reader.end()
    order=[next(i for i,r in enumerate(records)if r[1]==e['name'])for e in imports]
    requests=NS['C'].words(len(order))+b''.join(raw[i]for i in order)
    lines=['#include "native64-typed-module.h"']
    def array(name,data):lines.append('static const uchar '+name+'[]={'+','.join(map(str,data))+'};')
    array('file_requests',requests);array('file_names',structural['strings'])
    lines.append('static const uint file_hashes[]={'+','.join(str(e['signature'])+'U'for e in imports)+'};')
    lines.append('static const Native64TypedImportRequest file_tables[]={{{file_requests,file_names,sizeof(file_requests),sizeof(file_names)},file_hashes,2}};')
    array('file_unicode_names',NS['L']['pool'](['идентичность🚀','id\0extra']))
    for r in rows:
        n=r['name'];lines += [f'static const Native64TypedStep client_code_{n}[]={{']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};']
        array('client_map_'+n,r['frame']['map'])
        if r.get('externals'):lines += [f'static const Native64ModuleSite client_sites_{n}[]={{']+['{'+','.join(map(str,(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])))+'},'for e in r['externals']]+['};']
    args=next(r for r in rows if r['name']=='run')['externals'][0]['args'];size=(max(a[0]+a[3]for a in args)+7)&~7;array('request_map',[0]*((size+63)//64))
    lines+=['static const Native64ModuleImport client_requests[]={']+['{'+json.dumps(e['name'])+f',{e["signature"]}U,{size},request_map,{(size+63)//64}'+'},'for e in imports]+['};','static const Native64ModuleTable client_tables[]={{client_requests,2}};']
    exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1];exports={e['name']:e for e in exports}
    lines+=['static const Native64ModuleFunction client_functions[]={']
    for r in rows:
        n=r['name'];e=exports[n];assert e['pc']==r['first'];sites=r.get('externals',[])
        lines.append('{'+json.dumps(n)+f',{e["signature"]}U,1,client_code_{n},{len(r["code"])},{r["frame"]["bytes"]},client_map_{n},{len(r["frame"]["map"])},'+(f'client_sites_{n}'if sites else'nil')+f',{len(sites)}'+'},')
    lines+=['};'];r=rows[0];globals={g[0]:g[1]for g in r['globals']};assert set(globals)=={'p','nil','calls'}
    initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3];assert len(initial)==1 and next(iter(initial.values()))=={'kind':'word','low':17,'high':0}
    array('client_data_map',r['data']['map']);lines.append('static const Native64ModuleInit client_initial[]={{'+str(globals['calls'])+',4,17}};')
    lines.append('static const Native64ModuleSpec client_spec={"Client","trusted/Client",client_functions,4,~0U,'+f'{r["data"]["bytes"]},client_data_map,{len(r["data"]["map"])},client_initial,1,client_tables,1,nil,nil,0,read64ownedimport'+'};')
    for i,r in enumerate(rows):lines.append(f'static const uint client_{r["name"]}={i};')
    lines += [f'static const uint client_provider_offset={globals["p"]},client_calls_offset={globals["calls"]};',f'static const uint file_kind={kind};']
    (work/'typed-files.h').write_text('\n'.join(lines)+'\n')
    print('PASS file LOAD source:',scalar,'complete consumer and .m hashes/structural ordinal frames; complete provider proof includes uncalled failing init',flush=True)
    return image

def unit():
    source=NS['unit'](iload=True)
    source=source.replace('require(int condition)','filebaserequire(int condition,const char *expression,int line)',1)
    source=source.replace('checks);\n\t\tabort();','checks);fprintf(stderr,"typed file base predicate line=%d: %s\\n",line,expression);exit(10);',1)
    source=source.replace('/* Bound each allocation','#define require(c) filebaserequire(!!(c),#c,__LINE__)\n/* Bound each allocation',1)
    source=source.replace('int main(int argc,char**argv)','int oldfilemain(int argc,char**argv)',1)
    start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start);handlers=source[start:end]
    extract=NS['S']['FILES']['FILES']['function']
    source=source.replace(extract(source,'void\ndestroylinks('),extract((ROOT/'libinterp/link.c').read_text(),'void\ndestroylinks('),1)
    source=source.replace('#define require(c) libraryrequire','#undef require\n#define require(c) libraryrequire',1)
    source+='\n'+extract((ROOT/'libinterp/xec.c').read_text(),'OP(movp)')+'\n'+extract((ROOT/'libinterp/string.c').read_text(),'int\nstringcmp(')+'\n'
    source+='\n#if defined(__BIONIC__) || defined(TEST_PRIVATE_UNSIGNED_CONVERTER)\n'+''.join(line for line in (ROOT/'lib9/strtoull.c').read_text().splitlines(True)if not line.startswith('#include'))+'\n#endif\n'
    return source+'\n#line 1 "native64-typed-files.c"\n'+(ROOT/'utils/tests/native64-typed-files.c').read_text().replace(' NAMESPACE_HANDLERS',handlers)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
    if not __debug__:p.error('independent file/source oracles require assertions')
    with tempfile.TemporaryDirectory(prefix='typed-files-',dir=ROOT)as td:
        work=Path(td);file=work/'test.c';source=unit();file.write_text(source);cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(work)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'test';command=cc+[str(file),str(ROOT/'libinterp/dec.c')]+releaseflags(cc)+['-lm','-o',str(binary)]
        for kind in (1,2,3):
            image=generate(a.limbo.resolve(),work,kind);subprocess.run(command,check=True)
            result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(image)],capture_output=True,text=True,timeout=180)
            assert result.returncode==0 and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout,result.stderr)
            for line in result.stdout.splitlines():
                if line.startswith('PASS '):print(line,flush=True)
            if kind==1:
                result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(image),'default'],capture_output=True,text=True,timeout=180)
                assert result.returncode==0,(result.returncode,result.stdout,result.stderr)
                print(result.stdout.strip(),flush=True)
                controls=[('if(memcmp(&hash,&ldt[j].sig,4))return 0;',';','typed file install guard omitted: case=7',True),
                          ('for(k=0;k<(uint)type->np;k++)if(type->map[k])return 0;',';','typed file install guard omitted: case=13',True),
                          ('owner->tables[i].strings=at;',';','typed file predicate: source LOAD runtime',False)]
                for before,after,diagnostic,installonly in controls:
                    assert source.count(before)==1
                    file.write_text(source.replace(before,after,1));subprocess.run(command,check=True)
                    args=[str(binary),str(image)]+(['install-only']if installonly else[])
                    result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+args,capture_output=True,text=True,timeout=180)
                    assert result.returncode==10 and diagnostic in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout,result.stderr)
                    print('PASS typed file active control:',arch,diagnostic,flush=True)
                file.write_text(source)
        # Disabled builds must not dereference even deliberately invalid inputs.
        extract=NS['S']['FILES']['FILES']['function']
        disabled=work/'disabled.c';disabled.write_text('#include "lib9.h"\n#include "isa.h"\n#include "interp.h"\n'+extract((ROOT/'libinterp/readmod.c').read_text(),'int\ninstall64typedimports(')+'\nint main(void){return install64typedimports((Module*)H,(Native64TypedImportRequest*)H,65,2);}\n')
        out=work/'disabled';subprocess.run(cc+['-UDIS64_PILOT_LOADER','-DDIS64_PILOT_LOADER=0',str(disabled),'-o',str(out)],check=True)
        subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(out)],check=True,timeout=90)
        print('PASS typed file disabled installer:',arch,'no input dereference',flush=True)
if __name__=='__main__':main()
