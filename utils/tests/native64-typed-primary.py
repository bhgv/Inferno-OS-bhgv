#!/usr/bin/env python3
"""Execute the actual emuinit primary loadmod branch through a file Command.init."""
import argparse,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'))
EXT=CMD['EXT']

def generate(limbo,work):
    actual=(ROOT/'appl/cmd/emuinit.b').read_text()
    prefix=actual[actual.index('loadmod(args:'):actual.index('\n\t# if we can\'t load the module directly')]
    assert 'mod = load Command "/dis/wm/awm.dis";'in prefix and 'return (mod, args);'in prefix
    # Preserve the complete primary branch verbatim. The separate fallback's
    # variadic Sys diagnostics are outside this selected source slice.
    source='implement Primary;include "provider.m";Primary:module{init:fn(ctxt:ref Draw->Context,args:list of string);};init(ctxt:ref Draw->Context,args:list of string){mod:Command;(mod,args)=loadmod(args);mod->init(nil,args);}\n'+prefix+'\nraise "fail:error";\n}\n'
    data,rows=EXT['compile_source'](limbo,work,source,interface=CMD['INTERFACE'],loads=True)
    assert len(rows)==2 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    imports=EXT['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
    assert len(imports)==len(imports[0])==1 and imports[0][0]['name']=='init'
    EXT['MODULE']['check_exports'](data,rows)
    assert sum(CMD['verify_nilbranches'](data,r,imports[0],rows=rows)for r in rows)==2
    for key in ('path','pair','target'):
        changed=json.loads(json.dumps(rows));body=next(r for r in changed if r['name']=='loadmod')
        if key=='path':next(step for step in body['code']if step[0]==8)[2]+=8
        if key=='pair':next(step for step in body['code']if step[5]==56)[4]=32
        if key=='target':changed[0]['calls'][0][2]=999999
        try:
            for r in changed:CMD['verify_nilbranches'](data,r,imports[0],rows=changed)
        except (AssertionError,StopIteration):pass
        else:raise AssertionError('primary classic oracle missed '+key)
    lines=[]
    def array(name,values):lines.append('static const uchar '+name+'[]={'+','.join(map(str,values))+'};')
    for r in rows:
        n=r['name'];array('primary_map_'+n,r['frame']['map'])
        lines+=['static const Native64TypedStep primary_code_'+n+'[]={']+['{'+','.join(map(str,step))+'},'for step in r['code']]+['};']
        sites=[(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])for e in r.get('externals',[])]
        sites += [(a,b,next(i for i,row in enumerate(rows)if row['first']==target),contract,inherited,0,0,0,0)for a,b,target,desc,offset,contract,inherited in r.get('calls',[])]
        if sites:lines+=['static const Native64ModuleSite primary_sites_'+n+'[]={']+['{'+','.join(map(str,e))+'},'for e in sorted(sites)]+['};']
    exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1];exports={e['name']:e for e in exports}
    assert exports['init']['signature']==imports[0][0]['signature']
    lines+=['static const Native64ModuleFunction primary_functions[]={']
    for r in rows:
        n=r['name'];export=exports.get(n);sites=r.get('externals',[])+r.get('calls',[])
        lines+=['{'+json.dumps(n)+f',{export["signature"]if export else 0}U,{int(export is not None)},primary_code_{n},{len(r["code"])},{r["frame"]["bytes"]},primary_map_{n},{len(r["frame"]["map"])},'+('primary_sites_'+n if sites else'nil')+f',{len(sites)},nil,0,{48 if r.get("result_pair")else 0}'+'},']
    lines+=['};'];initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3];literals=[];layout=rows[0]['data']
    for old,cell in sorted(initial.items()):
        assert cell['kind']=='string'
        offset=next(leaf[1]for leaf in layout['leaves']if leaf[0]==old);text=bytes(cell['bytes']);name='primary_literal_'+str(old);array(name,text);literals.append((offset,text,name))
    assert {text for _,text,_ in literals}=={b'/dis/sh.dis',b'/dis/wm/awm.dis',b'-l',b'fail:error'}
    lines+=['static const Native64ModuleString primary_strings[]={']+['{'+f'{offset},{len(text)},{name}'+'},'for offset,text,name in literals]+['};']
    array('primary_data_map',layout['map'])
    lines+=['static const Native64ModuleSpec primary_spec={"Primary","trusted/Primary",primary_functions,2,~0U,'+f'{layout["bytes"]},primary_data_map,{len(layout["map"])},nil,0,command_tables,1,c2string,primary_strings,{len(literals)},read64ownedimport'+'};']
    (work/'primary.h').write_text('\n'.join(lines)+'\n')
    print('PASS primary source: verbatim emuinit primary loadmod branch, actual /dis/wm/awm.dis, direct Limbo init and three corrupt-classic-oracle controls',flush=True)
    return rows

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
    if not __debug__:p.error('source oracle requires assertions')
    with tempfile.TemporaryDirectory(prefix='typed-primary-',dir=ROOT)as td:
        work=Path(td);source=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
        before='int\nkopen(char *path, int mode)\n{';assert source.count(before)==1
        source=source.replace(before,'static int primaryrunning,primarypaths;\n'+before+'\n if(primaryrunning){commandpath(path);primarypaths++;}',1)
        # Define the path predicate before the namespace transport, avoiding a
        # forward dependency on the later Command fixture's diagnostics.
        source=source.replace('static int primaryrunning,primarypaths;','static void commandpath(char*p){if(strcmp(p,"/dis/wm/awm.dis")){fprintf(stderr,"primary predicate: exact AWM namespace path\\n");exit(10);}}\nstatic int primaryrunning,primarypaths;',1)
        begin=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',begin);handlers=source[begin:end]
        source+='\n'+(ROOT/'utils/tests/native64-typed-primary.c').read_text().replace(' PRIMARY_HANDLERS',handlers);src=work/'unit.c';src.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(work)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'unit';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        for implementation,fault in(('Command',False),('Wm',False),('Wm',True)):
            image=CMD['generate'](a.limbo.resolve(),work,implementation,fault,argv=True,local=True,pair=True,raises=True)
            rows=generate(a.limbo.resolve(),work);subprocess.run(command,check=True)
            r=subprocess.run(runner+[str(binary),str(image)],capture_output=True,text=True,timeout=180)
            assert not r.returncode and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
            print(r.stdout,end='',flush=True)
            if implementation=='Command':
                header=work/'primary.h';original=header.read_text()
                match=next(m for m in re.finditer(r'static const uchar primary_literal_\d+\[\]=\{([^}]+)\};',original)if bytes(map(int,m[1].split(',')))==b'/dis/wm/awm.dis')
                values=list(map(int,match[1].split(',')));values[0]=ord('.')
                path_control=original[:match.start(1)]+','.join(map(str,values))+original[match.end(1):]
                match=re.search(r'static const Native64TypedStep primary_code_init\[\]=\{(.*?)\};',original,re.S);assert match
                steps=match[1].strip().splitlines();index=rows[0]['externals'][0]['first']+1
                step=list(map(int,steps[index].strip('{},').split(',')));assert step[0]==41 and step[4:]==[64,64]
                step[1]=(step[1]&~56)|8;step[2]=rows[0]['args'][1][0];steps[index]='{'+','.join(map(str,step))+'},'
                context_control=original[:match.start(1)]+'\n'+'\n'.join(steps)+'\n'+original[match.end(1):]
                for altered,diagnostic in((path_control,'primary predicate: exact AWM namespace path'),(context_control,'Command file predicate: primary nil Context publication')):
                    header.write_text(altered);subprocess.run(command,check=True)
                    r=subprocess.run(runner+[str(binary),str(image)],capture_output=True,text=True,timeout=180)
                    assert r.returncode==10 and diagnostic in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
                    print('PASS primary active control:',diagnostic,flush=True)
                header.write_text(original)

if __name__=='__main__':main()
