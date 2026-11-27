#!/usr/bin/env python3
"""Compute mapped imported String arguments using the real addc and C/JIT calls."""
import argparse,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'));EXT=CMD['EXT']
INTERFACE='Paths:module{open:fn(path:string,mode:int):string;};'
SOURCE='''implement Concat;include "provider.m";
Concat:module{fresh:fn(p:Paths,name:string):string;forward:fn(p:Paths,name:string):string;reverse:fn(p:Paths,name:string):string;fp:fn(p:Paths,left:string,right:string):string;};
fresh(p:Paths,name:string):string{a:=p->open("#e/"+name,0);return a;}
forward(p:Paths,name:string):string{return p->open("#e/"+name,0);}
reverse(p:Paths,name:string):string{return p->open(name+"#e/",0);}
fp(p:Paths,left:string,right:string):string{return p->open(left+right,0);}
'''

def emit(limbo,work,fault):
    bad='z:=0;one:=1;v:=one/z;'
    provider='implement Paths;'+INTERFACE+'open(path:string,mode:int):string{'+(bad if fault else'if(mode!=0){'+bad+'}')+'return path;}'
    data,prows=EXT['compile_source'](limbo,work,provider,interface=INTERFACE,loads=True)
    assert len(prows)==1 and prows[0]['accepted'];EXT['BASE']['verify'](data,prows[0])
    probe='implement Probe;include "provider.m";Probe:module{init:fn();};init(){p:=load Paths "trusted/Paths";p->open(nil,0);}'
    probe,_=EXT['compile_source'](limbo,work,probe,interface=INTERFACE,loads=True)
    imports=EXT['BASE']['READ']['legacy_instructions'](probe,with_imports=True)[-1];assert len(imports)==len(imports[0])==1;imports=imports[0]
    data,rows=EXT['compile_source'](limbo,work,SOURCE,interface=INTERFACE,loads=True)
    assert len(rows)==4 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    EXT['MODULE']['check_exports'](data,rows)
    assert prows[0]['signature']==imports[0]['signature']
    for r in rows:EXT['verify'](data,r,imports,loads=True)
    _,closed=EXT['compile_source'](limbo,work,SOURCE,interface=INTERFACE,loads=False);assert all(not r['accepted']for r in closed)
    for key in('source','middle','dest','opcode'):
        changed=json.loads(json.dumps(rows[0]));index=next(i for i,step in enumerate(changed['code'])if step[0]==EXT['OPCODES']['IADDC'])
        if key=='source':changed['code'][index][2]+=8
        if key=='middle':changed['code'][index][3]+=8
        if key=='dest':changed['code'][index][4]+=8
        if key=='opcode':changed['code'][index][0]=41
        try:EXT['verify'](data,changed,imports,loads=True)
        except AssertionError:pass
        else:raise AssertionError('concat classic oracle missed '+key)
    lines=['#include "native64-typed-module.h"']
    def array(name,values):lines.append('static const uchar '+name+'[]={'+','.join(map(str,values or[0]))+'};')
    array('concat_request_map',[0,128]);lines+=['static const Native64ModuleImport concat_requests[]={{"open",'+str(imports[0]['signature'])+'U,80,concat_request_map,2}};','static const Native64ModuleTable concat_tables[]={{concat_requests,1}};']
    def module(prefix,data,rows):
        exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1];exports={e['name']:e for e in exports}
        for r in rows:
            n=prefix+'_'+r['name'];array(n+'_map',r['frame']['map'])
            lines.extend(['static const Native64TypedStep '+n+'_code[]={']+['{'+','.join(map(str,step))+'},'for step in r['code']]+['};'])
            if r.get('externals'):lines.extend(['static const Native64ModuleSite '+n+'_sites[]={']+['{'+','.join(map(str,(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])))+'},'for e in r['externals']]+['};'])
        lines.append('static const Native64ModuleFunction '+prefix+'_functions[]={')
        for r in rows:
            n=prefix+'_'+r['name'];e=exports[r['name']];sites=r.get('externals',[])
            lines.append('{'+json.dumps(r['name'])+f',{e["signature"]}U,1,{n}_code,{len(r["code"])},{r["frame"]["bytes"]},{n}_map,{len(r["frame"]["map"])},'+(n+'_sites'if sites else'nil')+f',{len(sites)}'+'},')
        lines.append('};');layout=rows[0]['data'];initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3];literals=[]
        for old,cell in sorted(initial.items()):
            assert cell['kind']=='string';offset=next(leaf[1]for leaf in layout['leaves']if leaf[0]==old);name=prefix+'_literal_'+str(old);text=bytes(cell['bytes']);array(name,text);literals.append((offset,text,name))
        if literals:lines.extend(['static const Native64ModuleString '+prefix+'_strings[]={']+['{'+f'{offset},{len(text)},{name}'+'},'for offset,text,name in literals]+['};'])
        array(prefix+'_data_map',layout['map'])
        lines.append('static const Native64ModuleSpec '+prefix+'_spec={"'+prefix+'","trusted/'+prefix+'",'+prefix+'_functions,'+str(len(rows))+',~0U,'+str(layout['bytes'])+','+prefix+'_data_map,'+str(len(layout['map']))+',nil,0,'+('concat_tables,1'if prefix=='concat_client'else'nil,0')+','+('c2string,'+prefix+'_strings,'+str(len(literals))if literals else'nil,nil,0')+'};')
    module('concat_client',data,rows)
    pdata,_=EXT['compile_source'](limbo,work,provider,interface=INTERFACE,loads=True);module('concat_provider',pdata,prows)
    positions=[];offset=0
    for r in rows:
        positions.append(offset+next(i for i,s in enumerate(r['code'])if s[0]==EXT['OPCODES']['IADDC']));offset+=len(r['code'])
    lines+=['static const uint concat_pcs[]={'+','.join(map(str,positions))+'};','static const uint concat_fault='+str(int(fault))+';']
    (work/'concat.h').write_text('\n'.join(lines)+'\n')
    print('PASS concat source: four complete String argument bodies, independent classic operands/maps/hashes, four corrupt reports, old-profile refusal',flush=True)
    return rows

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
    if not __debug__:p.error('source oracles require assertions')
    runpy.run_path(str(ROOT/'utils/tests/dis64-typed-concat-args.py'))['main']()
    with tempfile.TemporaryDirectory(prefix='typed-concat-',dir=ROOT)as td:
        w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True)
        source=CMD['unit']().replace('int main(int argc,char **argv)','int old_command_main(int argc,char **argv)')
        extract=CMD['NS']['S']['FILES']['FILES']['function'];strings=(ROOT/'libinterp/string.c').read_text()
        for sig in('String*\nstringdup(','void\ncvtup(','String*\naddstring(','OP(addc)'):source+='\n'+extract(strings,sig)+'\n'
        start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start)
        source+='\n'+(ROOT/'utils/tests/native64-typed-concat-args.c').read_text().replace(' CONCAT_HANDLERS',source[start:end]);src=w/'unit.c';src.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'));arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=w/'unit';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        for fault in(False,True):
            rows=emit(a.limbo.resolve(),w,fault);subprocess.run(command,check=True)
            r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180);assert not r.returncode and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr);print(r.stdout,end='',flush=True)
            if not fault:
                module=(ROOT/'include/native64-typed-module.h').read_text();before='map=modes[k]==AFP?f->map:s->datamap;bit=offsets[k]/8;\n                if(!(map[bit/8]&(0x80>>(bit%8))))return 0;';assert module.count(before)==1
                (w/'native64-typed-module.h').write_text(module.replace(before,';',1));subprocess.run(command,check=True)
                r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
                assert r.returncode==10 and 'concat metadata refusal removed: case=0'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                print('PASS concat active control: source root map required',flush=True);(w/'native64-typed-module.h').unlink()
                multi=(ROOT/'include/native64-typed-multi.h').read_text();before='bit/8<np && (map[bit/8]&(0x80>>(bit%8)))';assert multi.count(before)==1
                (w/'native64-typed-module.h').write_text(module)
                (w/'native64-typed-multi.h').write_text(multi.replace(before,'1',1));subprocess.run(command,check=True)
                r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
                assert r.returncode==10 and 'concat metadata refusal removed: case=1'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                print('PASS concat active control: destination argument root required',flush=True)
                before='if(s->op==IADDC)p->reg=s->m;';assert multi.count(before)==1
                (w/'native64-typed-multi.h').write_text(multi.replace(before,'if(s->op==IADDC)p->reg=s->m+8;',1));subprocess.run(command,check=True)
                r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
                assert r.returncode==10 and 'concat predicate: concat middle publication'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                print('PASS concat active control: exact middle operand publication',flush=True)
                (w/'native64-typed-module.h').unlink();(w/'native64-typed-multi.h').unlink()

if __name__=='__main__':main()
