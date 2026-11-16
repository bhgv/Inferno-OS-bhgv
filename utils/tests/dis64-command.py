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
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--materialize-only',action='store_true');p.add_argument('--chain',action='store_true');p.add_argument('--chain-owner-control',action='store_true');p.add_argument('--gc-control',choices=('callee-frame','context-map','mp-tail','codec-array','array-backing','index-owner','local-frame','local-result','local-argument'));p.add_argument('--literal-wire',choices=('empty','signed-runes'));p.add_argument('--reference-width-control',action='store_true');p.add_argument('--field-offset-control',choices=('source','destination'));p.add_argument('--local-result-map-control',action='store_true');p.add_argument('--scalar-result-map-control',action='store_true');p.add_argument('--case',action='append');p.add_argument('--implementation',default='Command');a=p.parse_args();env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
 try:CASES['implementation']('implement Command;include "command.m";',a.implementation)
 except ValueError as error:p.error(str(error))
 if a.chain_owner_control and not a.chain:p.error('--chain-owner-control requires --chain')
 if a.literal_wire and(not a.materialize_only or a.case!=['literals-context']):p.error('--literal-wire requires --materialize-only --case literals-context')
 with tempfile.TemporaryDirectory(prefix='command-init-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE']);(w/'library.m').write_text('Library:module{inc:fn(n:int):int;};')
  # Legacy fixture declarations are compile-only; live interface comes solely from files.
  (w/'library.h').write_bytes(subprocess.check_output([str(a.limbo.resolve()),'-H','-R','-M','Library',str(w/'library.m')],env=env))
  source=w/'test.c';text=P['unit'](codecs=True);
  if a.chain:
   text=text.replace('static uchar *clientfile,*providerfile;static unsigned int clientfilebytes,providerfilebytes;','static uchar *clientfile,*providerfile,*dependencyfile;static unsigned int clientfilebytes,providerfilebytes,dependencyfilebytes;static uchar dependencydata[1048576],providersaved[1048576];')
   old='else if(!strcmp(path,"library.dis64")){filesource=providerfile;filebytes=providerfilebytes;}else return -1;';assert text.count(old)==1;text=text.replace(old,old.replace('else return -1;','else if(!strcmp(path,"dependency.dis64")){filesource=dependencyfile;filebytes=dependencyfilebytes;}else return -1;'))
   text=text.replace('clientfilebytes=libraryread(argv[1]','dependencyfilebytes=libraryread(argv[argc-1],dependencydata,sizeof(dependencydata));dependencyfile=dependencydata;clientfilebytes=libraryread(argv[1]')
  if a.chain:
   text=text.replace('providerfile=provider;memmove(saved,client,clientfilebytes);','providerfile=provider;memmove(providersaved,provider,providerfilebytes);memmove(saved,client,clientfilebytes);')
   text=text.replace('namespaceend(p);clientempty();memmove(client,saved,clientfilebytes);','namespaceend(p);clientempty();memmove(provider,providersaved,providerfilebytes);memmove(client,saved,clientfilebytes);')
  if a.chain_owner_control:
   text='static unsigned char *chainborrow;\n'+text
   text=text.replace('providerfile=provider;','providerfile=provider;chainborrow=provider;')
   old='owner->tables[0]=(Native64ImportRequest){at,at+v->requests.bytes,v->requests.bytes,v->names.bytes};';assert text.count(old)==1
   text=text.replace(old,old+'if(v->importing)owner->tables[0]=(Native64ImportRequest){chainborrow+v->requests.offset,chainborrow+v->names.offset,v->requests.bytes,v->names.bytes};')
  text=text.replace('require(int condition)', 'baserequire(int condition,const char *expression,int line)',1).replace('checks);\n\t\tabort();','checks);fprintf(stderr,"line=%d predicate=%s\\n",line,expression);exit(10);',1);text=text.replace('/* Bound each allocation','#define require(c) baserequire((c),#c,__LINE__)\n/* Bound each allocation',1).replace('#define require(c) libraryrequire','#undef require\n#define require(c) libraryrequire');text=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-gc.py'))['instrument'](text);
  if a.chain:
   text=text.replace('free(read.dir);free(read.image);','free(read.dir);if(read.image)memset(read.image,0xa5,read.bytes);free(read.image);')
   old='clientlocalremember(ml->m,providerfile,providerfilebytes);';assert text.count(old)==1;text=text.replace(old,'if(!strcmp(ml->m->path,"dependency.dis64"))clientlocalremember(ml->m,dependencyfile,dependencyfilebytes);else '+old)
  text=text.replace('require(clientloads>0);return 0;','require(clientloads>0);fprintf(stderr,"index-pause-observations=%u\\n",clientindexpauses);fprintf(stderr,"local-frame-observations=%u\\n",clientlocalpauses);fprintf(stderr,"import-helper-observations=%u\\n",clientownpauses);fprintf(stderr,"managed-result-observations=%u\\n",clientresultpauses);return 0;')
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
  text=text.replace('clientloads++;','if(!strcmp(ml->m->path,"library.dis64"))commandproviderliterals(ml);clientloads++;')
  if a.chain:
   text=text.replace('commandproviderliterals(ml);','commandproviderliterals(ml);memset(providerfile,0xa5,providerfilebytes);')
  text=text.replace('observedispatch(m,jit);m->ss=', 'commandliteralcheck(&view,m->origmp);observedispatch(m,jit);m->ss=')
  if a.chain:
   text=text.replace('if(!strcmp(ml->m->path,"library.dis64"))commandproviderliterals(ml);memset(providerfile,0xa5,providerfilebytes);','if(!strcmp(ml->m->path,"library.dis64")){commandproviderliterals(ml);memset(providerfile,0xa5,providerfilebytes);}')
  if a.gc_control=='mp-tail':text=text.replace('clientproviderjit=cflag=pjit;','m->type[1]->mark=clientliteralmpmark;clientproviderjit=cflag=pjit;')
  if a.gc_control=='context-map':text=text.replace('commandcontextnomark=0','commandcontextnomark=1')
  if a.gc_control=='array-backing':text=text.replace('Tarray.mark=markarray;','Tarray.mark=noptrs;')
  if a.gc_control=='codec-array':
   assert text.count('clientloads++;')==1;text=text.replace('clientloads++;','ml->m->type[0]->mark=clientcodecframemark;clientloads++;')
  if a.gc_control=='index-owner':
   assert text.count('clientloads++;')==1;text=text.replace('clientloads++;','ml->m->type[0]->mark=clientindexframemark;clientloads++;')
  if a.gc_control=='local-result':
   text=text.replace('clientloads++;','ml->m->type[0]->mark=clientresultframemark;clientloads++;')
  if a.gc_control=='local-argument':
   text=text.replace('clientloads++;','for(unsigned int local=0;local<clientlocalcount;local++)clientlocaltypes[local]->mark=clientargumentframemark;clientloads++;')
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
  # Exercise Inferno conversion semantics too. Android/arm64 lib9.h maps these
  # calls to private symbols, so a standalone fixture must link their owners.
  binary=w/'test';subprocess.run(cc+[str(source),str(ROOT/'libinterp/dec.c'),str(ROOT/'lib9/strtoll.c'),str(ROOT/'lib9/strtoull.c'),'-Wl,--wrap=free','-lm','-o',str(binary)],env=env,check=True)
  rows=[r for r in (CASES['chaincases']()if a.chain else CASES['cases']())if not a.case or r[0]in a.case];assert rows
  for row in rows:
   name,provider,caller,failure=row[:4]
   provider=CASES['implementation'](provider,a.implementation)
   if a.chain:
    (w/'library.m').write_text(row[5]);(w/'dependency.b').write_text(row[4]);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'dependency.dis64'),str(w/'dependency.b')],env=env,capture_output=True,text=True);assert not r.returncode,(name,r.stderr)
   for filename,text,flag in [('command',provider,'-64b'),('client',caller,'-64c')]:
    (w/(filename+'.b')).write_text(text);subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/(filename+'.dis64')),str(w/(filename+'.b'))],env=env,check=True,capture_output=True)
   if a.literal_wire:
    helpers=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));parts=sections((w/'command.dis64').read_bytes());values=helpers['pool'](parts[7]);values[4]=[]if a.literal_wire=='empty'else[0,0x80000000,0xffffffff];parts[7]=helpers['poolbytes'](values);(w/'command.dis64').write_bytes(helpers['P']['rebuild'](parts))
   imported=sections((w/'client.dis64').read_bytes())[5];exported=sections((w/'command.dis64').read_bytes())[6]
   assert imported[16:]==exported[20:]and struct.unpack_from('>IIII',imported,16)==(0xface0186,expectedhash,2,0),(name,imported.hex(),exported.hex())
   r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'client.dis64'),str(w/'command.dis64'),'0','0',str(failure),'guarded']+(['context','fields']if name in('fields-native','branches-native','branches-native-loop','branches-native-mixed','constructors-native')else['context']if name=='context-non-nil'else[])+(['materialize']if a.materialize_only else[])+([str(w/'dependency.dis64')]if a.chain else[]),env=env,capture_output=True,text=True,timeout=120)
   if a.chain_owner_control:assert r.returncode==10 and 'client execution: error=division by zero'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   elif a.local_result_map_control or a.scalar_result_map_control:assert r.returncode==10 and 'client execution: error=bounds'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   elif a.reference_width_control:assert r.returncode==10 and 'reference width mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   elif a.field_offset_control:assert r.returncode==10 and 'Context field identity mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   elif a.gc_control:assert r.returncode==10 and 'client GC graph retention mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   else:
    assert not r.returncode,(name,r.returncode,r.stderr)
    if name.startswith(('index-','intarray-'))and not a.materialize_only:
     observed=re.search(r'index-pause-observations=(\d+)',r.stderr);assert observed and int(observed[1])>0,(name,r.stderr)
   if name.startswith(('result-','scalarresult-'))and not(a.gc_control or a.materialize_only or a.local_result_map_control or a.scalar_result_map_control or a.chain_owner_control):
    observed=re.search(r'managed-result-observations=(\d+)',r.stderr);assert observed and int(observed[1])>0,(name,r.stderr)
   if name.startswith(('local-','args-','scalarresult-','managedarg-'))and not(a.gc_control or a.materialize_only or a.local_result_map_control or a.scalar_result_map_control or a.chain_owner_control):
    observed=re.search(r'local-frame-observations=(\d+)',r.stderr);assert observed and int(observed[1])>0,(name,r.stderr)
   if name.startswith('importhelper-')and not(a.gc_control or a.materialize_only):
    observed=re.search(r'import-helper-observations=(\d+)',r.stderr);assert observed and int(observed[1])>0,(name,r.stderr)
   print('PASS ordinary .m init -> file LOAD -> managed MCALL/RET:',arch,name,flush=True)
  if a.implementation!='Command':print('PASS implementation label:',a.implementation,'; requested interface: Command',flush=True)
  print('PASS ordinary init pipeline:',arch,len(rows),'pairs; actual GC'+('; constructor-only'if a.materialize_only else'; semantic borrowed import owner omission'if a.chain_owner_control else'; semantic scalar-result map omission'if a.scalar_result_map_control else'; semantic managed-result map omission'if a.local_result_map_control else'; semantic '+a.gc_control+' omission'if a.gc_control else'; semantic '+a.field_offset_control+' offset omission'if a.field_offset_control else'; semantic reference width omission'if a.reference_width_control else';96 scheduler roots per pair'),flush=True)
if __name__=='__main__':main()
