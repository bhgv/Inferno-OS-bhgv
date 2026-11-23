#!/usr/bin/env python3
"""Checked Sys.fstat mixed results through actual MCALL, native binder and Sys handler."""
import argparse,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'));EXT=CMD['EXT']
SOURCE='''implement Stat;include "provider.m";
Stat:module{open:fn(path:string);probe:fn(fd:ref Sys->FD):int;length:fn(fd:ref Sys->FD):big;twice:fn(fd:ref Sys->FD):int;shifted:fn(fd:ref Sys->FD,marker:int):big;permission:fn(fd:ref Sys->FD):int;};sys:Sys;
open(path:string){sys=load Sys path;}
probe(fd:ref Sys->FD):int{(ok,d):=sys->fstat(fd);return ok;}
length(fd:ref Sys->FD):big{(ok,d):=sys->fstat(fd);if(ok<0)return big -1;return d.length;}
twice(fd:ref Sys->FD):int{ok:=0;for(i:=0;i<2;i++){(n,d):=sys->fstat(fd);ok=n;}return ok;}
shifted(fd:ref Sys->FD,marker:int):big{marker++;(ok,d):=sys->fstat(fd);if(ok<0)return big -1;return d.length+big marker;}
permission(fd:ref Sys->FD):int{return executable(sys->fstat(fd),8r444);}
'''
def generate(limbo,w,query="fstat"):
    shell=(ROOT/'appl/cmd/sh/sh.b').read_text()
    executable=re.search(r'(?m)^executable\([^\n]+\n\{.*?^\}',shell,re.S)[0]
    source=SOURCE+executable
    if query=="stat":source=source.replace("fd:ref Sys->FD","path:string").replace("sys->fstat(fd)","sys->stat(path)")
    signature=0x319328dd if query=="stat"else 0xda4499c2
    data,rows=EXT['compile_source'](limbo,w,source,interface=(ROOT/'module/sys.m').read_text(),loads=True)
    assert len(rows)==7 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    imports=EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
    assert imports==[[{'signature':signature,'name':query}]]
    EXT['MODULE']['check_exports'](data,rows)
    for r in rows:
        EXT['verify'](data,r,imports[0],loads=True,rows=rows)
        for e in r.get('externals',[]):
            assert e['name']==query and e['contract']==88 and not e['inherited']and e['args']==[[64,13 if query=='stat'else 12,4,8]]
            base=e['resultoffset'];expect=[(0,9,4),(8,13,8),(16,13,8),(24,13,8),(32,13,8),(40,4,8),(48,9,4),(52,9,4),(56,9,4),(60,9,4),(64,9,4),(72,4,8),(80,9,4),(84,9,4)]
            assert all(any(l[1]==base+off and l[3:]==[size,kind]for l in r['frame']['leaves'])for off,kind,size in expect)
    permission=next(r for r in rows if r['name']=='permission')
    assert any(step[0]==EXT['OPCODES']['IMOVMP'] and step[5]==177 for step in permission['code'])
    # Ordinary external lowering and unrelated tuple imports stay closed.
    _,closed=EXT['compile_source'](limbo,w,source,interface=(ROOT/'module/sys.m').read_text(),loads=False)
    assert all(not r['accepted']for r in closed)
    real=(ROOT/'module/sys.m').read_text()
    for badsource,badinterface in ((source.replace('Sys','Other'),real.replace('Sys: module','Other: module')),
        *(([(source,real.replace('fd:\tint;','fd:\tbig;'))])if query=='fstat'else[]),
        (source.replace('return d.length','return big d.length'), '\n'.join(l for l in real.replace('length:\tbig;','length:\tint;').splitlines()if not l.lstrip().startswith(('nulldir:','zerodir:'))))):
        _,badrows=EXT['compile_source'](limbo,w,badsource,interface=badinterface,loads=True)
        assert all(not r['accepted']for r in badrows if r['name'] not in ('open','executable'))
    lines=['#include "native64-typed-module.h"','static const uchar stat_request_map[]={0,128};','static const Native64ModuleImport stat_requests[]={{"fstat",0xda4499c2U,72,stat_request_map,2}};','static const Native64ModuleTable stat_tables[]={{stat_requests,1}};']
    exports={e['name']:e for e in EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1]}
    indices={r['first']:i for i,r in enumerate(rows)}
    for r in rows:
        n=r['name'];lines+=['static const Native64TypedStep stat_code_'+n+'[]={']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};','static const uchar stat_map_'+n+'[]={'+','.join(map(str,r['frame']['map']))+'};']
        sites=[(e['first'],e['end'],e['binding'],88,0,1,0,*e['module']) for e in r.get('externals',[])]
        sites.extend((first,end,indices[target],contract,inherited,0,0,0,0)
                     for first,end,target,_,_,contract,inherited in r.get('calls',[]))
        r['test_sites']=len(sites)
        if sites:lines+=['static const Native64ModuleSite stat_sites_'+n+'[]={']+['{'+','.join(map(str,e))+'},'for e in sorted(sites)]+['};']
    lines+=['static const Native64ModuleFunction stat_functions[]={']
    for r in rows:
        n=r['name'];lines+=['{"'+n+'",'+str(exports.get(n,{}).get('signature',0))+f'U,{int(n in exports)},stat_code_{n},{len(r["code"])},{r["frame"]["bytes"]},stat_map_{n},{len(r["frame"]["map"])},'+('stat_sites_'+n if r['test_sites']else'nil')+','+str(r['test_sites'])+'},']
    lines+=['};','static const uchar stat_data_map[]={'+','.join(map(str,rows[0]['data']['map']))+'};']
    initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3];values=[]
    for old,cell in initial.items():
        assert cell['kind'] in ('word','long')
        offset=next(l[1]for l in rows[0]['data']['leaves']if l[0]==old)
        bits=(cell['high']<<32)|cell['low']
        values.append('{'+f'{offset},{8 if cell["kind"]=="long" else 4},{bits}ULL'+'}')
    lines+=['static const Native64ModuleInit stat_initial[]={'+','.join(values)+'};','static const Native64ModuleSpec stat_spec={"Stat","trusted/Stat",stat_functions,nelem(stat_functions),~0U,'+str(rows[0]['data']['bytes'])+',stat_data_map,sizeof(stat_data_map),stat_initial,'+str(len(values))+',stat_tables,1,nil,nil,0,read64ownedimport};','static const uint stat_sys_offset='+str(next(g[1]for g in rows[0]['globals']if g[0]=='sys'))+';']
    (w/'stat.h').write_text(('\n'.join(lines)+'\n').replace('\"fstat\",0xda4499c2U','\"'+query+'\",'+hex(signature)+'U'))
    (w/'sys-native64.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Sys',str(ROOT/'module/sys.m')],text=True))
    print('PASS real Sys.'+query+' declarations: complete native 88-byte result, independent classic leaves/calls, source length read and old-profile refusal',flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--stat',action='store_true');a=p.parse_args()
    if not __debug__:p.error('source oracles require assertions')
    runpy.run_path(str(ROOT/'utils/tests/dis64-stat-result.py'))['main']()
    with tempfile.TemporaryDirectory(prefix='emuinit-fstat-',dir=ROOT)as td:
        w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w,'stat'if a.stat else'fstat')
        frontier=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](a.limbo.resolve(),'appl/cmd/emuinit.b',w)
        getenv=next(f for f in frontier['functions']if f['name']=='getenv')
        assert getenv['accepted']
        print('PASS unchanged real emuinit: complete getenv body accepted; full startup execution remains separate',flush=True)
        source=CMD['unit']().replace('int main(int argc,char **argv)','int old_command_main(int argc,char **argv)')
        extract=CMD['NS']['S']['FILES']['FILES']['function'];port=(ROOT/'emu/port/inferno.c').read_text()
        source+='\n#include "sys-native64.h"\nstatic Dir*statdirfstat(int);\nchar Ebadarg[]="bad arg";\nvoid seterror(char*s,...){kwerrstr("%s",s);}\n'
        vm=(ROOT/'libinterp/xec.c').read_text()
        heap=(ROOT/'libinterp/heap.c').read_text()
        source+='\n'+extract(heap,'void\nincmem(')+'\n'+extract(vm,'static int\nmappedcopytype(')+'\n'+extract(vm,'OP(movmp)')+'\n'
        for sig in('static int\nsysnativeint(','static int\nsysiofd(','static void\nsysstatusframe(','static void*\nsysresultowner(','static void\nsysdirowners(','static void\nunpackdir('):source+='\n'+extract(port,sig)+'\n'
        source+='\n'+extract((ROOT/'libinterp/runt.c').read_text(),'void\nretstr(')+'\n'
        source+='\n#define kdirfstat statdirfstat\n'+extract(port,'void\nSys_fstat(')+'\n#undef kdirfstat\n'
        if a.stat:
            source+='\nstatic Dir*statdirstat(char*p){require(!strcmp(p,"/native64/stat"));return statdirfstat(23); }\n#define kdirstat statdirstat\n'+extract(port,'void\nSys_stat(')+'\n#undef kdirstat\n'
        start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start)
        body=(ROOT/'utils/tests/native64-emuinit-fstat.c').read_text().replace(' STAT_HANDLERS',source[start:end])
        if a.stat:
            body=body.replace('Sys.fstat','Sys.stat').replace('F_Sys_fstat','F_Sys_stat').replace('O(F_Sys_stat,fd)','O(F_Sys_stat,s)').replace('Sys_fstat(fp)','Sys_stat(fp)').replace('"fstat"','"stat"').replace('0xda4499c2U','0x319328ddU').replace('if(i==14)request.name="stat"','if(i==14)request.name="fstat"')
            begin=body.index(' {Type*t=dtype(freeheap,8,nil,0);');end=body.index('\n outer=statlauncher',begin);body=body[:begin]+' fd=c2string("/native64/stat",14);'+body[end:]
        source+='\n#line 1 "native64-emuinit-fstat.c"\n'+body;src=w/'unit.c';src.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'));arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=w/'unit';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=180)
        subprocess.run(runner+[str(binary),'binder'],check=True,timeout=180)
        header=(ROOT/'include/native64-stat-result.h').read_text();before='if(bit/8>=np || !!(map[bit/8]&(0x80>>(bit%8)))!=(at>=8 && at<40))return 0;';assert header.count(before)==1
        (w/'native64-local-bind.h').write_text((ROOT/'include/native64-local-bind.h').read_text())
        (w/'native64-stat-result.h').write_text(header.replace(before,'if(bit/8>=np)return 0;',1))
        (w/'native64-typed-module.h').write_text((ROOT/'include/native64-typed-module.h').read_text())
        subprocess.run(command,check=True);r=subprocess.run(runner+[str(binary),'metadata'],capture_output=True,text=True,timeout=180)
        assert r.returncode==10 and 'fstat metadata refusal removed: case=0'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
        print('PASS fstat active control: constructor requires all four mapped String roots',flush=True)
        (w/'native64-stat-result.h').unlink();(w/'native64-typed-module.h').unlink()
        bind=(ROOT/'include/native64-local-bind.h').read_text();before='if(!!(bit/8<';assert bind.count(before)==1
        (w/'native64-local-bind.h').write_text(bind.replace(before,'if(contract!=88 && !!(bit/8<',1))
        subprocess.run(command,check=True);r=subprocess.run(runner+[str(binary),'binder'],capture_output=True,text=True,timeout=180)
        assert r.returncode==10 and 'fstat predicate: binder fourth root refused before builtin'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
        print('PASS fstat active control: runtime binder checks fourth root before builtin',flush=True)
if __name__=='__main__':main()
