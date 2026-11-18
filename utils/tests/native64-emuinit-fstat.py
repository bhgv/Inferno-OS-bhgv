#!/usr/bin/env python3
"""Checked Sys.fstat mixed results through actual MCALL, native binder and Sys handler."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'));EXT=CMD['EXT']
SOURCE='''implement Stat;include "provider.m";
Stat:module{open:fn(path:string);probe:fn(fd:ref Sys->FD):int;length:fn(fd:ref Sys->FD):big;twice:fn(fd:ref Sys->FD):int;shifted:fn(fd:ref Sys->FD,marker:int):big;};sys:Sys;
open(path:string){sys=load Sys path;}
probe(fd:ref Sys->FD):int{(ok,d):=sys->fstat(fd);return ok;}
length(fd:ref Sys->FD):big{(ok,d):=sys->fstat(fd);if(ok<0)return big -1;return d.length;}
twice(fd:ref Sys->FD):int{ok:=0;for(i:=0;i<2;i++){(n,d):=sys->fstat(fd);ok=n;}return ok;}
shifted(fd:ref Sys->FD,marker:int):big{marker++;(ok,d):=sys->fstat(fd);if(ok<0)return big -1;return d.length+big marker;}
'''
def generate(limbo,w):
    data,rows=EXT['compile_source'](limbo,w,SOURCE,interface=(ROOT/'module/sys.m').read_text(),loads=True)
    assert len(rows)==5 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    imports=EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
    assert imports==[[{'signature':0xda4499c2,'name':'fstat'}]]
    EXT['MODULE']['check_exports'](data,rows)
    for r in rows:
        EXT['verify'](data,r,imports[0],loads=True)
        for e in r.get('externals',[]):
            assert e['name']=='fstat'and e['contract']==88 and not e['inherited']and e['args']==[[64,12,4,8]]
            base=e['resultoffset'];expect=[(0,9,4),(8,13,8),(16,13,8),(24,13,8),(32,13,8),(40,4,8),(48,9,4),(52,9,4),(56,9,4),(60,9,4),(64,9,4),(72,4,8),(80,9,4),(84,9,4)]
            assert all(any(l[1]==base+off and l[3:]==[size,kind]for l in r['frame']['leaves'])for off,kind,size in expect)
    # Ordinary external lowering and unrelated tuple imports stay closed.
    _,closed=EXT['compile_source'](limbo,w,SOURCE,interface=(ROOT/'module/sys.m').read_text(),loads=False)
    assert all(not r['accepted']for r in closed)
    real=(ROOT/'module/sys.m').read_text()
    for badsource,badinterface in ((SOURCE.replace('sys->fstat(fd)','sys->stat(nil)'),real),
        (SOURCE.replace('Sys','Other'),real.replace('Sys: module','Other: module')),
        (SOURCE,real.replace('fd:\tint;','fd:\tbig;')),
        (SOURCE.replace('return d.length','return big d.length'), '\n'.join(l for l in real.replace('length:\tbig;','length:\tint;').splitlines()if not l.lstrip().startswith(('nulldir:','zerodir:'))))):
        _,badrows=EXT['compile_source'](limbo,w,badsource,interface=badinterface,loads=True)
        assert all(not r['accepted']for r in badrows if r['name']!='open')
    lines=['#include "native64-typed-module.h"','static const uchar stat_request_map[]={0,128};','static const Native64ModuleImport stat_requests[]={{"fstat",0xda4499c2U,72,stat_request_map,2}};','static const Native64ModuleTable stat_tables[]={{stat_requests,1}};']
    exports={e['name']:e for e in EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1]}
    for r in rows:
        n=r['name'];lines+=['static const Native64TypedStep stat_code_'+n+'[]={']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};','static const uchar stat_map_'+n+'[]={'+','.join(map(str,r['frame']['map']))+'};']
        sites=r.get('externals',[])
        if sites:lines+=['static const Native64ModuleSite stat_sites_'+n+'[]={']+['{'+','.join(map(str,(e['first'],e['end'],e['binding'],88,0,1,0,*e['module'])))+'},'for e in sites]+['};']
    lines+=['static const Native64ModuleFunction stat_functions[]={']
    for r in rows:
        n=r['name'];lines+=['{"'+n+'",'+str(exports[n]['signature'])+f'U,1,stat_code_{n},{len(r["code"])},{r["frame"]["bytes"]},stat_map_{n},{len(r["frame"]["map"])},'+('stat_sites_'+n if r.get('externals')else'nil')+','+str(len(r.get('externals',[])))+'},']
    lines+=['};','static const uchar stat_data_map[]={'+','.join(map(str,rows[0]['data']['map']))+'};']
    initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3];values=[]
    for old,cell in initial.items():
        assert cell['kind']=='long';offset=next(l[1]for l in rows[0]['data']['leaves']if l[0]==old);values.append('{'+f'{offset},8,~0ULL'+'}')
    lines+=['static const Native64ModuleInit stat_initial[]={'+','.join(values)+'};','static const Native64ModuleSpec stat_spec={"Stat","trusted/Stat",stat_functions,5,~0U,'+str(rows[0]['data']['bytes'])+',stat_data_map,sizeof(stat_data_map),stat_initial,'+str(len(values))+',stat_tables,1,nil,nil,0,read64ownedimport};','static const uint stat_sys_offset='+str(next(g[1]for g in rows[0]['globals']if g[0]=='sys'))+';']
    (w/'stat.h').write_text('\n'.join(lines)+'\n')
    (w/'sys-native64.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Sys',str(ROOT/'module/sys.m')],text=True))
    print('PASS real Sys.fstat declarations: complete native 88-byte result, independent classic leaves/calls, source length read and old-profile refusal',flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
    if not __debug__:p.error('source oracles require assertions')
    runpy.run_path(str(ROOT/'utils/tests/dis64-stat-result.py'))['main']()
    with tempfile.TemporaryDirectory(prefix='emuinit-fstat-',dir=ROOT)as td:
        w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w)
        frontier=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](a.limbo.resolve(),'appl/cmd/emuinit.b',w)
        getenv=next(f for f in frontier['functions']if f['name']=='getenv')
        assert getenv['accepted']
        print('PASS unchanged real emuinit: complete getenv body accepted; full startup execution remains separate',flush=True)
        source=CMD['unit']().replace('int main(int argc,char **argv)','int old_command_main(int argc,char **argv)')
        extract=CMD['NS']['S']['FILES']['FILES']['function'];port=(ROOT/'emu/port/inferno.c').read_text()
        source+='\n#include "sys-native64.h"\nstatic Dir*statdirfstat(int);\nchar Ebadarg[]="bad arg";\nvoid seterror(char*s,...){kwerrstr("%s",s);}\n'
        for sig in('static int\nsysnativeint(','static int\nsysiofd(','static void\nsysstatusframe(','static void*\nsysresultowner(','static void\nsysdirowners(','static void\nunpackdir('):source+='\n'+extract(port,sig)+'\n'
        source+='\n'+extract((ROOT/'libinterp/runt.c').read_text(),'void\nretstr(')+'\n'
        source+='\n#define kdirfstat statdirfstat\n'+extract(port,'void\nSys_fstat(')+'\n#undef kdirfstat\n'
        start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start)
        source+='\n#line 1 "native64-emuinit-fstat.c"\n'+(ROOT/'utils/tests/native64-emuinit-fstat.c').read_text().replace(' STAT_HANDLERS',source[start:end]);src=w/'unit.c';src.write_text(source)
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
