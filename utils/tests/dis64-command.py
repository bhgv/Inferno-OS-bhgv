#!/usr/bin/env python3
"""Compile .m-hashed ordinary init calls and execute file LOAD/MCALL with real GC."""
import argparse,os,re,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'));CASES=runpy.run_path(str(ROOT/'utils/tests/dis64-command-cases.py'))
def sections(data):
 d={}
 for i in range(struct.unpack_from('>I',data,28)[0]):
  k,f,at,n=struct.unpack_from('>HHII',data,32+12*i);assert f==1 and k not in d;d[k]=data[at:at+n]
 return d
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--materialize-only',action='store_true');p.add_argument('--gc-control',choices=('callee-frame','context-map','mp-tail','codec-array','array-backing','index-owner','local-frame','local-result'));p.add_argument('--literal-wire',choices=('empty','signed-runes'));p.add_argument('--reference-width-control',action='store_true');p.add_argument('--field-offset-control',choices=('source','destination'));p.add_argument('--local-result-map-control',action='store_true');p.add_argument('--scalar-result-map-control',action='store_true');p.add_argument('--case',action='append');a=p.parse_args();env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
 if a.literal_wire and(not a.materialize_only or a.case!=['literals-context']):p.error('--literal-wire requires --materialize-only --case literals-context')
 with tempfile.TemporaryDirectory(prefix='command-init-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE']);(w/'library.m').write_text('Library:module{inc:fn(n:int):int;};')
  # Legacy fixture declarations are compile-only; live interface comes solely from files.
  (w/'library.h').write_bytes(subprocess.check_output([str(a.limbo.resolve()),'-H','-R','-M','Library',str(w/'library.m')],env=env))
  source=w/'test.c';text=P['unit'](codecs=True);text=text.replace('require(int condition)', 'baserequire(int condition,const char *expression,int line)',1).replace('checks);\n\t\tabort();','checks);fprintf(stderr,"line=%d predicate=%s\\n",line,expression);exit(10);',1);text=text.replace('/* Bound each allocation','#define require(c) baserequire((c),#c,__LINE__)\n/* Bound each allocation',1).replace('#define require(c) libraryrequire','#undef require\n#define require(c) libraryrequire');text=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-gc.py'))['instrument'](text);
  text=text.replace('require(clientloads>0);return 0;','require(clientloads>0);fprintf(stderr,"index-pause-observations=%u\\n",clientindexpauses);fprintf(stderr,"local-frame-observations=%u\\n",clientlocalpauses);fprintf(stderr,"managed-result-observations=%u\\n",clientresultpauses);return 0;')
  text=text.replace('clientloads++;','clientlocalremember(ml->m,providerfile,providerfilebytes);clientloads++;')
  if a.local_result_map_control or a.scalar_result_map_control:
   old='m->prog[i].reg=v->localcount?dis64clientlocalcontract(v,i):8;';assert text.count(old)==1;text=text.replace(old,'m->prog[i].reg=24;'if a.scalar_result_map_control else'm->prog[i].reg=8;')
  header=subprocess.check_output([str(a.limbo.resolve()),'-H','-M','Command','-I',str(ROOT/'module'),str(w/'command.m')],env=env,text=True)
  (w/'command-generated.h').write_text(header)
  width=(ROOT/'utils/tests/dis64-reference-width.c').read_text()
  if a.reference_width_control:width=width.replace('actual_bnel();','actual_bnew();').replace('actual_beql();','actual_beqw();')
  hook=width+'\n'+'#include "command-generated.h"\n'+(ROOT/'utils/tests/dis64-command-context.c').read_text()
  at=text.index('/* Both caller CODE and provider CODE');text=text[:at]+hook+'\n'+text[at:]
  text=text.replace('argc<=8','argc<=10').replace('guarded|=!strcmp(argv[i],"guarded");','commandfields|=!strcmp(argv[i],"fields");commandcontext|=!strcmp(argv[i],"context");guarded|=!strcmp(argv[i],"guarded");')
  head,tail=text.rsplit('root=p->R.FP;',1);text=head+'root=p->R.FP;if(commandcontext)commandcontextseed(&view,root);'+tail
  marker=' for(jit=0;jit<2;jit++){\n  cflag=jit;allocations=0;m=readmod("client.dis64"'
  assert text.count(marker)==1;text=text.replace(marker,' commandreferencewidth();commandproviderfaults(&view);\n'+marker)
  text=text.replace('clientloads++;','commandproviderliterals(ml);clientloads++;')
  text=text.replace('observedispatch(m,jit);m->ss=', 'commandliteralcheck(&view,m->origmp);observedispatch(m,jit);m->ss=')
  if a.gc_control=='mp-tail':text=text.replace('clientproviderjit=cflag=pjit;','m->type[1]->mark=clientliteralmpmark;clientproviderjit=cflag=pjit;')
  if a.gc_control=='context-map':text=text.replace('commandcontextnomark=0','commandcontextnomark=1')
  if a.gc_control=='array-backing':text=text.replace('Tarray.mark=markarray;','Tarray.mark=noptrs;')
  if a.gc_control=='codec-array':
   assert text.count('clientloads++;')==1;text=text.replace('clientloads++;','ml->m->type[0]->mark=clientcodecframemark;clientloads++;')
  if a.gc_control=='index-owner':
   assert text.count('clientloads++;')==1;text=text.replace('clientloads++;','ml->m->type[0]->mark=clientindexframemark;clientloads++;')
  if a.gc_control=='local-result':
   text=text.replace('clientloads++;','ml->m->type[0]->mark=clientresultframemark;clientloads++;')
  if a.gc_control=='local-frame':
   text=text.replace('clientloads++;','for(unsigned int local=0;local<clientlocalcount;local++)clientlocaltypes[local]->mark=noptrs;clientloads++;')
  if a.gc_control=='callee-frame':
   assert text.count('clientloads++;')==1;text=text.replace('clientloads++;','ml->m->type[0]->mark=noptrs;clientloads++;')
  if a.field_offset_control:
   selected='s'if a.field_offset_control=='source'else'd';old='m->prog[i].'+selected+'.i.s=d->secondary;'
   assert text.count(old)==1;text=text.replace(old,'m->prog[i].'+selected+'.i.s=' + ('0;'if selected=='s'else'(d->secondary<24?0:d->secondary);'))
  assert text.count('namespaceend(p);clientempty();')==1;text=text.replace('namespaceend(p);clientempty();','commandfieldcheck();namespaceend(p);clientempty();')
  source.write_text(text)
  # Existing generated .m C table and the file signatures must use one hash.
  hashes=re.findall(r'(0x[0-9a-fA-F]+)U,\s*// Existing Limbo signature hash',header);assert hashes,header[-2000:];expectedhash=int(hashes[0],16)
  cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch=='arm64':cc+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
  elif arch=='riscv64':cc+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
  elif arch!='amd64':raise ValueError(arch)
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="library.h"','-DNAMESPACE_ORDINAL=0']+['-I'+str(x)for x in [w]+[ROOT/x for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]]
  binary=w/'test';subprocess.run(cc+[str(source),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)],env=env,check=True)
  rows=[r for r in CASES['cases']()if not a.case or r[0]in a.case];assert rows
  for name,provider,caller,failure in rows:
   for filename,text,flag in [('command',provider,'-64b'),('client',caller,'-64c')]:
    (w/(filename+'.b')).write_text(text);subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/(filename+'.dis64')),str(w/(filename+'.b'))],env=env,check=True,capture_output=True)
   if a.literal_wire:
    helpers=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));parts=sections((w/'command.dis64').read_bytes());values=helpers['pool'](parts[7]);values[4]=[]if a.literal_wire=='empty'else[0,0x80000000,0xffffffff];parts[7]=helpers['poolbytes'](values);(w/'command.dis64').write_bytes(helpers['P']['rebuild'](parts))
   imported=sections((w/'client.dis64').read_bytes())[5];exported=sections((w/'command.dis64').read_bytes())[6]
   assert imported[16:]==exported[20:]and struct.unpack_from('>IIII',imported,16)==(0xface0186,expectedhash,2,0),(name,imported.hex(),exported.hex())
   r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'client.dis64'),str(w/'command.dis64'),'0','0',str(failure),'guarded']+(['context','fields']if name in('fields-native','branches-native','branches-native-loop','branches-native-mixed','constructors-native')else['context']if name=='context-non-nil'else[])+(['materialize']if a.materialize_only else[]),env=env,capture_output=True,text=True,timeout=120)
   if a.local_result_map_control or a.scalar_result_map_control:assert r.returncode==10 and 'client execution: error=bounds'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   elif a.reference_width_control:assert r.returncode==10 and 'reference width mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   elif a.field_offset_control:assert r.returncode==10 and 'Context field identity mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   elif a.gc_control:assert r.returncode==10 and 'client GC graph retention mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   else:
    assert not r.returncode,(name,r.returncode,r.stderr)
    if name.startswith(('index-','intarray-'))and not a.materialize_only:
     observed=re.search(r'index-pause-observations=(\d+)',r.stderr);assert observed and int(observed[1])>0,(name,r.stderr)
   if name.startswith(('result-','scalarresult-'))and not(a.gc_control or a.materialize_only or a.local_result_map_control or a.scalar_result_map_control):
    observed=re.search(r'managed-result-observations=(\d+)',r.stderr);assert observed and int(observed[1])>0,(name,r.stderr)
   if name.startswith(('local-','args-','scalarresult-'))and not(a.gc_control or a.materialize_only or a.local_result_map_control or a.scalar_result_map_control):
    observed=re.search(r'local-frame-observations=(\d+)',r.stderr);assert observed and int(observed[1])>0,(name,r.stderr)
   print('PASS ordinary .m init -> file LOAD -> managed MCALL/RET:',arch,name,flush=True)
  print('PASS ordinary init pipeline:',arch,len(rows),'pairs; actual GC'+('; constructor-only'if a.materialize_only else'; semantic scalar-result map omission'if a.scalar_result_map_control else'; semantic managed-result map omission'if a.local_result_map_control else'; semantic '+a.gc_control+' omission'if a.gc_control else'; semantic '+a.field_offset_control+' offset omission'if a.field_offset_control else'; semantic reference width omission'if a.reference_width_control else';96 scheduler roots per pair'),flush=True)
if __name__=='__main__':main()
