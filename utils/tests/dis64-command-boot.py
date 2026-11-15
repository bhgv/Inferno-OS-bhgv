#!/usr/bin/env python3
"""Execute ordinary Command.init files in the complete namespace/heap/scheduler."""
import argparse,os,re,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];C=runpy.run_path(str(ROOT/'utils/tests/dis64-command-cases.py'));B=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('limbo','emu','root'):p.add_argument('--'+name,required=True,type=Path)
 p.add_argument('--runner',default='');a=p.parse_args();env=dict(os.environ);command=shlex.split(a.runner)+[str(a.emu.resolve()),'-v','-r',str(a.root.resolve())];runs=0
 with tempfile.TemporaryDirectory(prefix='command-boot-',dir=a.root.resolve())as td:
  w=Path(td);env['TMPDIR']=str(w);provider=w/'library.dis64';caller=w/'client.dis64';vmp='/'+str(provider.relative_to(a.root.resolve()));vmc='/'+str(caller.relative_to(a.root.resolve()));(w/'command.m').write_text(C['INTERFACE'])
  def compile(providertext,callertext):
   for text,name,flag,target in [(providertext,'command','-64b',provider),(callertext.replace('"library.dis64"','"'+vmp+'"'),'client','-64c',caller)]:
    (w/(name+'.b')).write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(target),str(w/(name+'.b'))],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
  def execute(jit,owner='Client',error=''):
   output=B['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmc]);assert 'native64 scalar module '+owner+': jit='+str(jit)+', exit="'+error+'"'in output and B['RETIRED']in output and 'panic:'not in output and 'faults:'not in output and ('Broken:'in output)==bool(error),output
  for name,text,client,failure in C['cases'](len(vmp)):
   if name in('context-non-nil','fields-native','branches-native','branches-native-loop','branches-native-mixed','constructors-native'):continue # Separate native C-seeded record witness, not guest construction.
   # A valid provider must actually link; a nil-returning importer alone cannot
   # distinguish a successful no-op init from admission refusal.
   compile(text,client.replace('if(m==nil)return;','if(m==nil){z:=0;bad:=1/z;}'))
   for jit in (0,1):execute(jit,'Command'if failure else'Client','dereference of nil'if failure==3 else'zero divide'if failure else'');runs+=1
   saved=provider.read_bytes();compile(text,client);provider.unlink()
   for jit in (0,1):execute(jit);runs+=1
   provider.write_bytes(saved);print('PASS full OS ordinary init:',name,'C/RX + missing file',flush=True)
  # A valid wrong hash must refuse before the explicitly failing init can run.
  _,text,client,_=next(row for row in C['cases'](len(vmp))if row[0]=='explicit-init-error');compile(text,client);bad=bytearray(provider.read_bytes())
  for i in range(struct.unpack_from('>I',bad,28)[0]):
   kind,flags,at,size=struct.unpack_from('>HHII',bad,32+12*i)
   if kind==6:bad[at+24]^=1
  provider.write_bytes(bad)
  for jit in (0,1):execute(jit)
  for path,flags in [(vmc,[]),(vmp,['-X','dis64-calls'])]:
   output=B['execute'](command+flags+['-d',path]);assert 'panic: loading 'in output and B['RETIRED']not in output,output
  examples=re.findall(r'```limbo\n(.*?)```',(ROOT/'doc/dis64-command-init.md').read_text(),re.S);assert len(examples)==3
  (w/'command.m').write_text(examples[0]);compile(examples[1],examples[2].replace("if(m == nil) return;","if(m == nil){z:=0;bad:=1/z;}"))
  for jit in (0,1):execute(jit)
  print('PASS ordinary init RU/EN tutorial: C/RX',flush=True)
  print('PASS ordinary init full OS:',runs,'main C/RX roots + hash/default/entryless refusal controls',flush=True)
if __name__=='__main__':main()
