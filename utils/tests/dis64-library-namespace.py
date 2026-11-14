#!/usr/bin/env python3
"""Compile .m requests and load whole provider files through real namespace admission."""
import argparse,os,re,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
L=runpy.run_path(str(ROOT/'utils/tests/dis64-library-files.py'));S=L['S'];C=L['C']
def arrays(text):
 result={}
 for name,body in re.findall(r'static const unsigned char (limbo_native64_\w+_scalar_(?:imports|strings))\[\] = \{([^}]+)\};',text):result[name.rsplit('_',1)[1]]=bytes(int(v,16)for v in re.findall(r'0x([0-9a-fA-F]{2})',body))
 assert set(result)=={'imports','strings'};return result

def declarations(text):return text.split('};',1)[0].split('implement Library;',1)[1]+'};'
def bindings(data):
 nr=C.BASE.Reader(data['strings']);names=[''.join(chr(nr.word())for _ in range(nr.word()))for _ in range(nr.word())];r=C.BASE.Reader(data['imports']);rows=[]
 for _ in range(r.word()):
  module,name,length=[r.word()for _ in range(3)];sig=r.take(length);values=tuple(int.from_bytes(sig[i:i+4],'big')for i in range(0,len(sig),4));assert values[0]==1 and values[2]==len(values)-3
  rows.append((names[module],names[name],values[1],values[3:]))
 r.end();nr.end();return rows

def unit(control=None,iload=False,iloadcontrol=None):
 s=L['unit']();extract=S['FILES']['FILES']['function'];src=(ROOT/'libinterp/readmod.c').read_text()
 s=s.replace('int main(int argc,char**argv)','int memorylibrarymain(int argc,char**argv)',1)
 s=s.replace('in->s.ind=64;in->d.ind=40;','in->reg=namespaceordinal;in->s.ind=64;in->d.ind=40;',1).replace('in->s.ind=40;in->d.ind=64;','in->reg=namespaceordinal;in->s.ind=40;in->d.ind=64;',1)
 pos=s.index('int\nkopen(');s=s[:pos]+'static int namespacefault,namespaceoverride,namespaceconstructors,namespaceacquiremissed;static vlong namespacelength;static unsigned int namespaceordinal;\n'+s[pos:]
 f=extract(s,'int\nkopen(');s=s.replace(f,f.replace('\n{','\n{\n if(namespacefault==1)return -1;\n if(namespacefault==2)error("namespace open error");',1),1)
 f=extract(s,'Dir*\nkdirfstat(');s=s.replace(f,f.replace('\n{','\n{\n if(namespacefault==3)return nil;\n if(namespacefault==4)error("namespace stat error");',1).replace('directory->length = filebytes;','directory->length = namespaceoverride?namespacelength:filebytes;'),1)
 f=extract(s,'long\nkread(');s=s.replace(f,f.replace('\n{','\n{\n if(namespacefault==10)dontcompile=1;\n if(namespacefault==8)return requested+1;\n if(fileposition){if(namespacefault==5)return 0;if(namespacefault==6)return -1;if(namespacefault==7)error("namespace read error");}',1),1)
 f=extract(s,'int\nkclose(');s=s.replace(f,f.replace('return 0;','return namespacefault==9?-1:0;'),1)
 f=extract(s,'int\nmustbesigned(');s=s.replace(f,f.replace('\n{','\n{\n if(worker.iprog!=nil){namespaceacquiremissed=1;acquire();error("namespace exclusion missing");}\n require(!fileopen);',1),1)
 s=s.replace('Modlink*\nprepare64calllibrarylink(','Modlink*\nactualprepare64calllibrarylink(',1)
 s+='''
Modlink*prepare64calllibrarylink(char*path,uchar*image,ulong bytes,int jit,const uchar*requests,ulong requestbytes,const uchar*strings,ulong stringbytes){
 require(!fileopen && worker.iprog==nil);namespaceconstructors++;
 return actualprepare64calllibrarylink(path,image,bytes,jit,requests,requestbytes,strings,stringbytes);
}
'''
 begin=src.index('/* Native scalar-library namespace bridge.');end=src.index('#if defined(DIS_NATIVE64_SCALAR_BOOT) && defined(DIS64_PILOT_LOADER)',begin);reader=src[begin:end]
 if control:
  before,after={'admission':('if(mustbesigned(read->path,read->image,read->bytes,read->dir))','if(0)'),
   'close':('if(read.fd>=0)kclose(read.fd);','if(0)kclose(read.fd);'),
   'acquire':('if(sync)acquire();','if(0)acquire();'),
   'profile':('if(!dis64launchmode() ||','if(0 ||')}[control]
  assert reader.count(before)==1;reader=reader.replace(before,after)
 s+='\n'+reader
 start=s.rfind(' optab[IMOVW]=countmovw;');end=s.index(' require(prepare64calls(',start);handlers=s[start:end]
 fixture=(ROOT/'utils/tests/dis64-library-namespace.c').read_text().replace('static unsigned int namespaceordinal;','').replace(' NAMESPACE_HANDLERS',handlers)
 if iload:
  fixture=fixture.replace('int main(int argc,char**argv)','int oldnamespace_main(int argc,char**argv)',1)
  s+='\n'+extract(xec:=(ROOT/'libinterp/xec.c').read_text(),'static Import*\nmoduleimports(')+'\n'+extract(xec,'OP(iload)')+'\n'
  s+='\nchar exNegsize[]="negative size";\nint hmsize(void*h){for(int i=0;i<RuntimeBlocks;i++)if(blocks[i].pointer==(uchar*)h-O(Bhdr,u.data))return blocks[i].bytes-O(Bhdr,u.data);require(0);return 0;}\n'+extract((ROOT/'libinterp/string.c').read_text(),'String*\nnewstring(')+'\n'
  fixture+='\n'+(ROOT/'utils/tests/dis64-library-iload.c').read_text().replace(' NAMESPACE_HANDLERS',handlers)
 result=s+'\n'+fixture
 if iloadcontrol:
  old,new={'copy':('owner->tables[i].imports=at;','/* active borrowed-request control */'),
   'pin':('ptrdel(D2H(ml));','/* active retained-pin control */'),
   'route':('structural = m->native64importload != nil;','structural = 0;')}[iloadcontrol]
  if iloadcontrol=='pin':
   body=extract(result,'OP(iload)');assert body.count(old)==1;result=result.replace(body,body.replace(old,new),1)
  else:assert result.count(old)==1;result=result.replace(old,new)
 return result

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--baseline',type=Path);p.add_argument('--materialize-only',action='store_true');p.add_argument('--disabled',action='store_true');p.add_argument('--iload',action='store_true');p.add_argument('--iload-control',choices=('copy','pin','route'));p.add_argument('--control',choices=('admission','close','acquire','profile'));a=p.parse_args();
 if a.iload and (a.control or a.disabled):p.error('--iload runs independently of reader controls/disabled gate')
 if a.iload_control and (not a.iload or a.materialize_only):p.error('--iload-control requires --iload instruction execution')
 env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
 with tempfile.TemporaryDirectory(prefix='library-namespace-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);source=w/'namespace.c';source.write_text(unit(a.control,a.iload,a.iload_control));cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch=='arm64':cc+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
  elif arch=='riscv64':cc+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
  elif arch!='amd64':raise RuntimeError(arch)
  cc+=['-DDIS_WORD32=1']+['-I'+str(x)for x in [w]+[ROOT/x for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]]
  runner=shlex.split(env.get('NATIVE64_RUNNER',''));rows=L['cases']()[:1]if a.control or a.disabled or a.iload_control else L['cases']()
  for name,text,fn,kind,args,values,expected,failure in rows:
   b=w/(name+'.b');b.write_text(text);m=w/(name+'.m');m.write_text(declarations(text));image=w/(name+'.dis64')
   S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-64b','-o',str(image),str(b)],env=env,capture_output=True,check=True)
   h=S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-H','-R','-M','Library',str(m)],env=env,capture_output=True,check=True,text=True).stdout
   plain=S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-H','-M','Library',str(m)],env=env,capture_output=True,check=True,text=True).stdout
   assert h.startswith(plain)
   if a.baseline:
    old=S['TYPED']['READ']['checked_run']([str(a.baseline.resolve()),'-H','-M','Library',str(m)],env=env,capture_output=True,check=True,text=True).stdout;assert plain==old
   payload=arrays(h);records=bindings(payload);ordinal=records.index(('Library',fn,kind,tuple(args)));assert len(records)==len(set(x[1]for x in records))
   header=w/(name+'.h');header.write_text(h);binary=w/name
   command=cc+['-DDIS64_PILOT_LOADER='+('0'if a.disabled else'1'),'-DNAMESPACE_HEADER="'+header.name+'"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_'+fn,str(source),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)];subprocess.run(command,env=env,check=True)
   valueargs=[str(struct.unpack('>Q',struct.pack('>d',v))[0]if k==3 else v&((1<<64)-1))for k,v in zip(args,values)]
   argv=[str(image),str(kind),str(expected),str(failure),str(ordinal),'materialize'if a.materialize_only else'run','selected']+valueargs
   if a.control=='profile'or a.disabled:argv[6]='default'
   r=subprocess.run(runner+[str(binary)]+argv,env=env,capture_output=True,text=True,timeout=120)
   if a.iload_control:
    assert r.returncode==10 and 'library fixture predicate:'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(a.iload_control,r.returncode,r.stderr)
    print('PASS ILOAD active control:',arch,a.iload_control,flush=True);return
   if a.control:
    diagnostic={'admission':'namespace admission route','close':'namespace descriptor cleanup','acquire':'namespace acquisition order','profile':'namespace startup profile'}[a.control]
    assert r.returncode==10 and ('library semantic failure: '+diagnostic)in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(a.control,r.returncode,r.stderr)
    print('PASS namespace active control:',arch,a.control,flush=True);return
   assert r.returncode==0,(name,r.returncode,r.stderr)
   if a.disabled:print('PASS disabled namespace bridge: no namespace/constructor work',flush=True);return
   if a.materialize_only:continue
   argv[6]='default';subprocess.run(runner+[str(binary)]+argv,env=env,check=True,capture_output=True,timeout=120)
   print('PASS compiler .m -> '+('ILOAD -> 'if a.iload else'')+'namespace -> admitted library -> scheduler:',arch,name,flush=True)
  if a.iload and a.materialize_only:
   print('PASS owned structural requests under materialization-only:',arch,len(rows),flush=True);return
  for label,case,transform in [('unused','single',lambda text:text.replace('};','unused:fn(n:int):int;};')),('wrong-kind','big',lambda text:text.replace('big','real'))]:
   name,text,fn,kind,args,values,expected,failure=next(x for x in L['cases']()if x[0]==case)
   m=w/(label+'.m');m.write_text(transform(declarations(text)))
   h=S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-H','-R','-M','Library',str(m)],env=env,capture_output=True,check=True,text=True).stdout
   records=bindings(arrays(h));ordinal=next(i for i,x in enumerate(records)if x[1]==fn)
   if label=='unused':assert any(x[1]=='unused'for x in records)
   else:assert records[ordinal][2:]==(3,(3,))
   header=w/(label+'.h');header.write_text(h);binary=w/label
   subprocess.run(cc+['-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="'+header.name+'"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_'+fn,str(source),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)],env=env,check=True)
   argv=[str(w/(name+'.dis64')),str(kind),str(expected),str(failure),str(ordinal),'reject','selected']+[str(v&((1<<64)-1))for v in values]
   r=subprocess.run(runner+[str(binary)]+argv,env=env,capture_output=True,text=True,timeout=120);assert r.returncode==0,(label,r.returncode,r.stderr)
   print('PASS independent .m namespace binding refusal:',arch,label,flush=True)
  print('PASS native namespace library pipeline:',arch,len(rows),'compiler interfaces/providers',flush=True)
if __name__=='__main__':main()
