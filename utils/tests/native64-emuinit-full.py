#!/usr/bin/env python3
"""Compile complete unchanged emuinit and execute its checked module with controlled services."""
import argparse,copy,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'));E=CMD['EXT']
def checked_catches(data,rows):
 _,code,types,_,_,_,_,classic=E['BASE']['READ']['legacy_instructions'](data,with_catches=True)
 verified=[]
 for i,row in enumerate(rows):
  first=row['first'];end=first+len(row['code'])
  selected=[c for c in classic if first<=c[1]<end]
  reported=row.get('catches',[])
  assert len(selected)==len(reported)
  for record, (slot,start,stop,cleanup,typed,arms,fallback) in zip(reported,selected):
   assert not typed and fallback==-1 and len(arms)==1
   leaves={leaf[0]:leaf[1] for leaf in row['frame']['leaves'] if leaf[2:4]==[4,8]}
   pattern,target=arms[0]
   clear=[]
   if cleanup!=-1:
    size,bits=types[cleanup]
    clear=[0]*len(row['frame']['map'])
    for b,mask in enumerate(bits):
     for j in range(8):
      if mask&(128>>j):
       old=4*(8*b+j);assert old<size
       native=leaves[old];clear[native//64]|=128>>(native//8%8)
   expected=dict(first=start-first,end=stop-first,target=target-first,slot=leaves[slot],pattern=pattern,cleanup=clear)
   assert record==expected,(row['name'],record,expected)
   verified.append((i,record))
 assert len(verified)==len(classic)
 return verified

def generate(limbo,w,source=None,module='Emuinit',shape=None,import_frames=None,export_signatures=None,extra_imports=None,source_label=None):
 expected_functions,expected_tables,expected_sites,entry=shape or ((4,3,3,'0')if module=='Emuinit'else(8,1,1,'~0U'))
 source=(ROOT/'appl/cmd/emuinit.b').read_text()if source is None else source;data,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
 assert len(rows)==expected_functions and all(r['accepted']for r in rows),[(r['name'],r.get('reason'),r.get('pc'))for r in rows]
 imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]+(extra_imports or [])
 proofs=copy.deepcopy(rows)
 if export_signatures is not None:
  for r in proofs:
   if r['signature']is None:
    assert r.get('signature_available')is False
    if r['export_name']in export_signatures:r['signature']=export_signatures[r['export_name']]
   elif r['export_name']in export_signatures:assert r['signature']==export_signatures[r['export_name']]
 E['MODULE']['check_exports'](data,proofs)
 for r in rows:CMD['verify_nilbranches'](data,r,imports,rows=rows)
 # Independent decoder must detect corruption in every new variadic record field.
 controls=0
 for row in rows:
  for j,site in enumerate(row.get('variadics',[])):
   for key in ('first','end','binding','module','resultoffset','contract','operation','count','inherited','frame'):
    corrupt=copy.deepcopy(row);v=corrupt['variadics'][j]
    if key=='module':v[key][1]+=8
    elif key=='frame':v[key]['bytes']+=8
    else:v[key]+=1
    try:CMD['verify_nilbranches'](data,corrupt,imports,rows=rows)
    except (AssertionError,IndexError,KeyError):controls+=1
    else:raise AssertionError('undetected variadic record corruption: '+key)
 print('PASS complete emuinit independent oracle:',controls,'variadic record corruptions detected',flush=True)
 # The old profile still closes FRAME/MCALL variadics; classic bytes are identical.
 old,closed=E['compile_source'](limbo,w,source,interface='',loads=False);assert old==data
 if module=='Emuinit':assert all(not r['accepted']for r in closed)
 headers={name:subprocess.check_output([str(limbo),'-H','-M',name,str(ROOT/'module'/filename)],text=True)for name,filename in [('Sys','sys.m'),('Arg','arg.m')]}
 (w/'sys-native64.h').write_text(headers['Sys'])
 lines=['static Modlink*fullload(Module*,WORD,char*);']
 def array(n,values):lines.append('static const uchar '+n+'[]={'+','.join(map(str,values or [0]))+'};')
 # Canonical generated frame geometry, not a guessed argument count.
 frames={}
 for name,h in headers.items():
  for fn,size in re.findall(r'^#define F_'+name+r'_(\w+)_size (\d+)',h,re.M):
   m=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M);assert m
   frames[(name,fn)]=(int(size),[int(x,0)for x in m[1].split(',')if x])
 assert len(imports)==expected_tables
 namespaces=[]
 for ti,table in enumerate(imports):
  namespace='Sys'if any(e['name']in('fprint','sprint','aprint','print')for e in table)else'Arg'if any(e['name']=='opt'for e in table)else'Command'
  namespaces.append({'Sys':0,'Arg':1,'Command':2}[namespace])
  for j,e in enumerate(table):
   size,mapping=import_frames[(e['name'],e['signature'])]if import_frames is not None else(80,[0,192])if namespace=='Command'else frames[(namespace,e['name'])]
   frames[(ti,j)]=(size,mapping);array(f'full_import_map_{ti}_{j}',mapping)
  lines+=['static const Native64ModuleImport full_import_'+str(ti)+'[]={']+['{'+json.dumps(e['name'])+f',{e["signature"]}U,{frames[(ti,j)][0]},full_import_map_{ti}_{j},{len(frames[(ti,j)][1])},{next((x['contract']for r in rows for x in r.get('externals',[])if x.get('contract')in(12,16,48,72,80,92,96,112,116,124)and x['name']==e['name']and x['signature']==e['signature']),0)}'+'},'for j,e in enumerate(table)]+['};']
 lines+=['static const Native64ModuleTable full_tables[]={']+[f'{{full_import_{ti},{len(t)}}},'for ti,t in enumerate(imports)]+['};']
 lines+=['static const uint full_namespaces[]={'+','.join(map(str,namespaces))+'};']
 for r in rows:
  n=r.get('export_name',r['name']).replace('.','_');array('full_map_'+n,r['frame']['map']);lines+=['static const Native64TypedStep full_code_'+n+'[]={']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};']
  sites=[]
  for e in r.get('externals',[]):
   tables=[i for i,t in enumerate(imports)if e['binding']<len(t)and t[e['binding']]=={'name':e['name'],'signature':e['signature']}]
   if len(tables)>1:
    # Equal signatures such as several modules' init() do not identify a namespace.
    # Match the exact mapped module operand to its source-proved LOAD table.
    mode,offset=e['module'];scope=rows if mode==0 else[r]
    loaded={c[3]for body in scope for c in body['code']if c[0]==E['OPCODES']['ILOAD']and c[1]&7==mode and c[4]==offset}
    tables=[i for i in tables if i in loaded]
   assert len(tables)==1,(r['name'],e['name'],e['module'],tables)
   sites.append((e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,tables[0],*e['module']))
  sites += [(a,b,next(i for i,x in enumerate(rows)if x['first']==target),contract,inherited,0,0,0,0)for a,b,target,desc,off,contract,inherited in r.get('calls',[])]
  if sites:lines+=['static const Native64ModuleSite full_sites_'+n+'[]={']+['{'+','.join(map(str,s))+'},'for s in sorted(sites)]+['};']
  for j,t in enumerate(r.get('tables',[])):
   assert t['op']in(3,14,144)
   case_rows=[]
   for k,(a,b,c)in enumerate(t['rows']):
    if t['op']==144:
     start='full_case_start_'+n+'_'+str(j)+'_'+str(k);array(start,list(a.encode()))
     stop='nil'
     if b is not None:stop='full_case_stop_'+n+'_'+str(j)+'_'+str(k);array(stop,list(b.encode()))
     case_rows.append('{0,0,'+str(c)+','+start+','+stop+','+str(len(a.encode()))+','+str(len(b.encode())if b is not None else 0)+'},')
    else:case_rows.append('{'+f'{a},{b},{c}'+'},')
   lines+=['static const Native64ModuleCaseRow full_rows_'+n+'_'+str(j)+'[]={']+case_rows+['};']
  if r.get('tables'):lines+=['static const Native64ModuleCase full_cases_'+n+'[]={']+['{'+f'{t["pc"]},{t["offset"]},{t["op"]},{len(t["rows"])},{t["fallback"]},full_rows_{n}_{j}'+'},'for j,t in enumerate(r['tables'])]+['};']
 exports={e['name']:e for e in E['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1]}
 lines+=['static const Native64ModuleFunction full_functions[]={']
 for r in rows:
  n=r.get('export_name',r['name']).replace('.','_');e=exports.get(r.get('export_name',n));nsites=len(r.get('externals',[]))+len(r.get('calls',[]));nt=len(r.get('tables',[]))
  lines+=['{'+json.dumps(r.get('export_name',r['name']))+f',{e["signature"]if e else 0}U,{int(e is not None)},full_code_{n},{len(r["code"])},{r["frame"]["bytes"]},full_map_{n},{len(r["frame"]["map"])},'+('full_sites_'+n if nsites else'nil')+f',{nsites},'+('full_cases_'+n if nt else'nil')+f',{nt},{r['result_aggregate']if r.get('result_aggregate')else 12 if r.get("result_words")else 16 if r.get("result_mixed")else 72 if r.get("result_channels")else 48 if r.get("result_pair")else 96 if r.get("result_geometry")==8 else 112 if r.get("result_geometry")==16 else 0}'+'},']
 lines+=['};'];array('full_data_map',rows[0]['data']['map']);initial=E['BASE']['READ']['legacy_instructions'](data,with_types=True,with_data=True)[3]
 strings=[];values=[];stringarrays=[];wordarrays=[];layout=rows[0]['data'];ranges=[(t['offset'],t['offset']+(4+4*len(t['rows'])if t['op']==3 else 12+24*len(t['rows'])if t['op']==144 and t['rows']else 8+12*len(t['rows'])))for r in rows for t in r.get('tables',[])]
 for old,cell in sorted(initial.items()):
  leaf=next(l for l in layout['leaves']if l[0]<=old<l[0]+l[2]);offset=leaf[1]+old-leaf[0]
  if any(a<=offset<b for a,b in ranges):continue
  if cell['kind']=='array':
   descriptor=E['BASE']['READ']['legacy_instructions'](data,with_types=True)[2][cell['descriptor']]
   if descriptor[0]==4 and not any(descriptor[1]):
    assert 0<=cell['length']<=65536 and all(k%4==0 and 0<=k<4*cell['length'] and v['kind']=='word'for k,v in cell['initial'].items())
    name=f'full_word_values_{old}';words=[cell['initial'].get(4*k,{}).get('low',0)&0xffffffff for k in range(cell['length'])]
    lines.append('static const UWORD '+name+'[]={'+','.join(str(v)+'U'for v in words or [0])+'};')
    wordarrays.append('{'+f'{offset},{cell["length"]},{name}'+'}');continue
   assert descriptor==(4,bytes([128]))or descriptor==(4,[128]),descriptor
   assert set(cell['initial'])==set(range(0,cell['length']*4,4))
   elements=[]
   for k,item in sorted(cell['initial'].items()):
    assert item['kind']=='string';name=f'full_array_{old}_{k}';text=bytes(item['bytes']);array(name,list(text));elements.append('{'+f'{2*k},{len(text)},{name}'+'}')
   name=f'full_array_values_{old}';lines.append('static const Native64ModuleString '+name+'[]={'+','.join(elements or ['{0,0,nil}'])+'};');stringarrays.append('{'+f'{offset},{cell["length"]},{name}'+'}');continue
  if cell['kind']=='string':
   name='full_literal_'+str(old);text=bytes(cell['bytes']);array(name,list(text));strings.append('{'+f'{offset},{len(text)},{name}'+'}')
  else:
   width={'byte':1,'word':4,'long':8,'real':8}[cell['kind']];bits=cell.get('high',0)<<32|cell['low'];values.append('{'+f'{offset},{width},{bits}ULL'+'}')
 lines+=['static const Native64ModuleString full_strings[]={'+','.join(strings)+'};','static const Native64ModuleInit full_initial[]={'+','.join(values or ['{0,0,0}'])+'};']
 lines+=['static const Native64ModuleStringArray full_arrays[]={'+','.join(stringarrays or ['{0,0,nil}'])+'};']
 lines+=['static const Native64ModuleWordArray full_wordarrays[]={'+','.join(wordarrays or ['{0,0,nil}'])+'};']
 if stringarrays:lines+=['static Array*fullarray(unsigned int count){return H2D(Array*,heaparray(&Tptr,count));}']
 catches=[]
 for i,h in checked_catches(data,rows):
  cleanup='nil'
  if h['cleanup']:
   cleanup=f'full_catch_cleanup_{i}';array(cleanup,h['cleanup'])
  catches.append('{'+f'{i},{h["first"]},{h["end"]},{h["target"]},{h["slot"]},'+json.dumps(h['pattern'])+f',{cleanup},{len(h["cleanup"])}'+'}')
 if catches:lines+=['static const Native64ModuleCatch full_catches[]={'+','.join(catches)+'};']
 lines+=['static const Native64ModuleSpec full_spec={'+json.dumps(module)+','+json.dumps('trusted/'+module)+',full_functions,'+str(len(rows))+','+entry+','+f'{layout["bytes"]},full_data_map,{len(layout["map"])},full_initial,{len(values)},full_tables,{len(imports)},c2string,full_strings,{len(strings)},fullload,full_arrays,{len(stringarrays)},'+('fullarray'if stringarrays else'nil')+(','+f'full_catches,{len(catches)}'if catches else',nil,0')+',full_wordarrays,'+str(len(wordarrays))+'};']
 variables=[]
 for i,r in enumerate(rows):
  for j,v in enumerate(r.get('variadics',[])):
   fixed=1 if v['operation']!=3 else 2;tail=v['frame']['leaves'][fixed:];slotname=f'full_var_slots_{i}_{j}'
   if tail:
    kinds={4:'Native64TailBig',7:'Native64TailReal',9:'Native64TailInt',16:'Native64TailFix',13:'Native64TailRef'}
    lines+=['static const Native64TailSlot '+slotname+'[]={'+','.join('{'+f'{kinds[l[4]]},{8*k},8,'+('&Tstring'if l[4]==13 else'nil')+'}'for k,l in enumerate(tail))+'};']
   name=f'full_var_map_{i}_{j}';array(name,v['frame']['map']);variables.append('{'+','.join(map(str,(i,v['first'],v['end'],v['binding'],*v['module'],v['resultoffset'],v['contract'],v['operation'],v['count'],v['frame']['bytes'])))+','+name+','+str(len(v['frame']['map']))+','+(slotname if tail else'nil')+','+str(v.get('inherited',0))+'}')
 assert len(variables)==expected_sites
 lines+=['static const Native64ModuleVariadic full_variadics[]={'+','.join(variables)+'};']
 for i,r in enumerate(rows):lines+=[f'static const uint full_fn_{r.get("export_name",r["name"]).replace(".","_")}={i};']
 (w/'full.h').write_text('\n'.join(lines)+'\n')
 print('PASS '+(source_label or 'complete unchanged '+module)+': '+str(len(rows))+' checked bodies, '+str(len(variables))+' variadic sites, complete classic operand/table/descriptor/export oracles and identical old-profile bytes',flush=True)
 return rows

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--generate-only',action='store_true');a=p.parse_args()
 if not __debug__:p.error('source oracles require assertions')
 runpy.run_path(str(ROOT/'utils/tests/dis64-variadic.py'))['main']()
 with tempfile.TemporaryDirectory(prefix='full-emuinit-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w)
  if a.generate_only:return
  original=(w/'full.h').read_text();argrows=generate(a.limbo.resolve(),w,(ROOT/'appl/lib/arg.b').read_text(),'Arg');(w/'arg.h').write_text((w/'full.h').read_text().replace('full_','arg_source_'));(w/'full.h').write_text(original)
  with (w/'arg.h').open('a')as stream:stream.write('static const uint arg_source_scalar='+str(next(g[1]for g in argrows[0]['globals']if g[0]=='printusage'))+';\n')
  probe='implement Printprobe;include "sys.m";Printprobe:module{init:fn();};init(){sys:=load Sys Sys->PATH;sys->print("%s\\t%s\\n","load","builtin");}'
  generate(a.limbo.resolve(),w,probe,'Printprobe',(1,1,1,'0'),source_label='Sys.print source probe')
  (w/'printprobe.h').write_text((w/'full.h').read_text().replace('full_','print_source_').replace('fullload','printload'))
  (w/'full.h').write_text(original)
  src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
  src+='\n#define LIMBO_NATIVE64_PREFIXES\n#include "sys-native64.h"\n#include "native64-sys-frame.h"\n#include "native64-sys-sites.h"\n#include "native64-sys-program.h"\n#include "native64-sys-module.h"\n'
  extract=CMD['NS']['S']['FILES']['FILES']['function'];runt=(ROOT/'libinterp/runt.c').read_text()
  src+='\n'+extract((ROOT/'emu/port/errstr.c').read_text(),'void\nkgerrstr(')+'\n'
  xec=(ROOT/'libinterp/xec.c').read_text()
  src+='\n'+extract(xec,'static Inst*\ncasepc(')+'\n'+extract(xec,'OP(icase)')+'\n'
  string=(ROOT/'libinterp/string.c').read_text()
  for sig in('String*\nstringdup(','void\ncvtup(','String*\naddstring(','OP(addc)'):src+='\n'+extract(string,sig)+'\n'
  for sig in ('int\nSys_setprogram64(','void\nSys_programcall64(','int\nSys_clearframes64('):src+='\n'+extract(runt,sig)+'\n'
  start=src.index(' optab[IMOVW]=countmovw;');stop=src.index(' require(prepare64calls(',start)
  src+='\n#line 1 "native64-emuinit-full.c"\n'+(ROOT/'utils/tests/native64-emuinit-full.c').read_text().replace(' FULL_HANDLERS',src[start:stop])
  f=w/'unit.c';f.write_text(src);arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  elif arch!='amd64':p.error(arch)
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(f),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True);runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
  subprocess.run(runner+[str(binary)],check=True,timeout=180)
  # The metadata suite must fail when the frame-map predicate is disabled.
  header=w/'native64-sys-module.h';original=(ROOT/'include/native64-sys-module.h').read_text();before='!native64modulemap(p->bytes,p->map,p->mapbytes,1)';assert original.count(before)==1
  # Also remove the exact root walk: otherwise its independent check masks this control.
  changed=original.replace(before,'0',1);before='if(!!(p->map[bit/8]&(128>>(bit%8)))!=reference)return 0;';assert changed.count(before)==1
  header.write_text(changed.replace(before,'if(0)return 0;',1));subprocess.run(command,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
  assert r.returncode==10 and 'metadata refusal before allocation/publication'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
  print('PASS complete emuinit active control: removed native frame-map validation detected',flush=True)
  header.unlink();header=w/'native64-typed-module.h';original=(ROOT/'include/native64-typed-module.h').read_text()
  start=original.index('for(j=0;j<f->count;j++)if(f->code[j].returned!=153 && (f->code[j].op==IBEQC')
  stop=original.index('for(j=0;j<f->count;j++)if(native64typedbranch',start)
  region=original[start:stop];before='if(!(map[bit/8]&(128>>(bit%8))))return 0;';assert region.count(before)==1
  header.write_text(original[:start]+region.replace(before,'if(0)return 0;',1)+original[stop:]);subprocess.run(command,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
  assert r.returncode==10 and 'malformed Arg String roles refused'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
  print('PASS Arg active control: removed String branch root-map validation detected',flush=True)

if __name__=='__main__':main()
