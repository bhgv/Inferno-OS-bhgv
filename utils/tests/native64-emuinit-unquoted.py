#!/usr/bin/env python3
"""Execute the unchanged recursive unquoted function from real emuinit source."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'));EXT=CMD['EXT']
CASES=[(b'',[]),(b' \t\n',[]),(b'a b',[b'a',b'b']),(b' a\tb\nc ',[b'a',b'b',b'c']),
       (b"one 'two three' four",[b'one',b'two three',b'four']),(b"'a''b'",[b"a'b"]),
       (b"a 'b",[b'a',b'b']),(b"'two words",[b'two words']),(b"ab'cd ef'gh",[b'abcd efgh']),
       ('мир 🚀'.encode(),['мир'.encode(),'🚀'.encode()]),(b'a\0b c',[b'a\0b',b'c']),
       (b"''",[b'']),(b"a '' b",[b'a',b'',b'b']),(b"a\r\nb",[b'a\r',b'b']),(None,[])]

def generate(limbo,w):
    actual=(ROOT/'appl/cmd/emuinit.b').read_text();body=actual[actual.index('unquoted(s:'):]
    source='implement Parse;Parse:module{parse:fn(s:string):list of string;};parse(s:string):list of string{return unquoted(s);}\n'+body
    data,rows=EXT['compile_source'](limbo,w,source,interface='',loads=True)
    assert len(rows)==2 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
    for r in rows:EXT['verify'](data,r,[],loads=True,rows=rows)
    for kind in ('IINDC','ISLICEC','IADDC','self-target'):
        changed=json.loads(json.dumps(rows));r=changed[1]
        if kind=='self-target':r['calls'][0][2]=changed[0]['first']
        else:next(step for step in r['code']if step[0]==EXT['OPCODES'][kind]and not step[5])[2]+=4
        try:
            for r in changed:EXT['verify'](data,r,[],loads=True,rows=changed)
        except (AssertionError,StopIteration):pass
        else:raise AssertionError('real source oracle missed '+kind)
    _,closed=EXT['compile_source'](limbo,w,source,interface='',loads=False);assert all(not r['accepted']for r in closed)
    nontail=source.replace('return unquoted(s + \"\'\");','ret:=unquoted(s + \"\'\");if(len ret==0)return nil;return ret;')
    assert nontail!=source
    _,bad=EXT['compile_source'](limbo,w,nontail,interface='',loads=True);assert all(not r['accepted']for r in bad)
    EXT['MODULE']['check_exports'](data,rows)
    lines=['#include "native64-typed-module.h"']
    def raw(name,b):lines.append('static const uchar '+name+'[]={'+','.join(map(str,b or b'\0'))+'};')
    for r in rows:
        n=r['name'];raw('parse_map_'+n,bytes(r['frame']['map']))
        lines+=['static const Native64TypedStep parse_code_'+n+'[]={']+['{'+','.join(map(str,s))+'},'for s in r['code']]+['};']
        sites=r.get('calls',[])
        if sites:lines+=['static const Native64ModuleSite parse_sites_'+n+'[]={']+['{'+','.join(map(str,(a,b,next(i for i,x in enumerate(rows)if x['first']==target),contract,inherited)))+'},'for a,b,target,desc,offset,contract,inherited in sites]+['};']
    exports=EXT['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1];exports={x['name']:x for x in exports}
    lines+=['static const Native64ModuleFunction parse_functions[]={']
    for r in rows:
        n=r['name'];e=exports.get(n);sites=r.get('calls',[])
        lines+=['{'+json.dumps(n)+f',{e["signature"]if e else 0}U,{int(bool(e))},parse_code_{n},{len(r["code"])},{r["frame"]["bytes"]},parse_map_{n},{len(r["frame"]["map"])},'+('parse_sites_'+n if sites else'nil')+f',{len(sites)}'+'},']
    lines+=['};'];layout=rows[0]['data'];initial=EXT['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3];strings=[]
    for old,cell in sorted(initial.items()):
        assert cell['kind']=='string';text=bytes(cell['bytes']);name='parse_literal_'+str(old);raw(name,text);offset=next(l[1]for l in layout['leaves']if l[0]==old);strings.append((offset,text,name))
    lines+=['static const Native64ModuleString parse_strings[]={']+['{'+f'{offset},{len(text)},{name}'+'},'for offset,text,name in strings]+['};'];raw('parse_data_map',bytes(layout['map']))
    lines+=['static const Native64ModuleSpec parse_spec={"EmuinitUnquoted","trusted/EmuinitUnquoted",parse_functions,2,~0U,'+f'{layout["bytes"]},parse_data_map,{len(layout["map"])},nil,0,nil,0,c2string,parse_strings,{len(strings)}'+'};',f'static const uint unquoted_entry={len(rows[0]["code"])};']
    for i,(text,expected)in enumerate(CASES):
        raw('input_'+str(i),text or b'')
        for j,word in enumerate(expected):raw(f'expected_{i}_{j}',word)
        lines+=['static const Expected words_'+str(i)+'[]={']+['{'+f'{len(word)},expected_{i}_{j}'+'},'for j,word in enumerate(expected)]+(['{0,nil},']if not expected else[])+['};']
    lines+=['static const ParseCase parse_cases[]={']+['{'+f'{len(text or b"")},input_{i},{len(expected)},words_{i},{int(text is None)}'+'},'for i,(text,expected)in enumerate(CASES)]+['};']
    (w/'unquoted.h').write_text('\n'.join(lines)+'\n');print('PASS unchanged real emuinit.unquoted source, complete local/self-tail call proof and independent classic relocation',flush=True)
    return rows

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
    if not __debug__:p.error('source proofs require assertions')
    runpy.run_path(str(ROOT/'utils/tests/dis64-self-tail.py'))['main']()
    with tempfile.TemporaryDirectory(dir=ROOT,prefix='real-unquoted-')as td:
        w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);rows=generate(a.limbo.resolve(),w)
        source=CMD['unit']().replace('int main(int argc,char **argv)','int old_command_main(int argc,char **argv)');extract=CMD['NS']['S']['FILES']['FILES']['function'];strings=(ROOT/'libinterp/string.c').read_text()
        for sig in('String*\nstringdup(','void\ncvtup(','String*\naddstring(','String*\nslicer(','OP(addc)','OP(indc)','OP(slicec)'):source+='\n'+extract(strings,sig)+'\n'
        start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start)
        source+='\n'+(ROOT/'utils/tests/native64-emuinit-unquoted.c').read_text().replace(' PARSE_HANDLERS',source[start:end]);src=w/'unit.c';src.write_text(source)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'));arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':p.error(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=w/'unit';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True)
        r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180);assert not r.returncode and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr);print(r.stdout,end='',flush=True)
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        module=(ROOT/'include/native64-typed-module.h').read_text();before='if(!!(map[bit/8]&(0x80>>(bit%8)))!=roots[k])return 0;';assert module.count(before)==1
        (w/'native64-typed-module.h').write_text(module.replace(before,';',1));subprocess.run(command,check=True)
        r=subprocess.run(runner+[str(binary),'metadata'],capture_output=True,text=True,timeout=180)
        assert r.returncode==10 and 'real unquoted metadata refusal removed: case=0'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
        print('PASS real parser active control: direct String operation root map required',flush=True)
        (w/'native64-typed-module.h').write_text(module)
        multi=(ROOT/'include/native64-typed-multi.h').read_text();before='p->d.i.s=s->d;';assert multi.count(before)==1
        (w/'native64-typed-multi.h').write_text(multi.replace(before,'p->d.i.s=(!c->external && c->inherited && c->target==out && j==c->end-1)?48:s->d;',1));subprocess.run(command,check=True)
        r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
        assert r.returncode==10 and 'real unquoted predicate: inherited result pointer'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
        print('PASS real parser active control: recursive borrowed result pointer checked before CALL',flush=True)

if __name__=='__main__':main()
