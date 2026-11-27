#!/usr/bin/env python3
"""Run Client -> importing Command.init -> dependency in the complete VM."""
import argparse,os,re,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];CASES=runpy.run_path(str(ROOT/'utils/tests/dis64-command-chain-cases.py'));BOOT=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('limbo','emu','root'):p.add_argument('--'+name,required=True,type=Path)
 p.add_argument('--runner',default='');a=p.parse_args();env=dict(os.environ);runs=0;command=shlex.split(a.runner)+[str(a.emu.resolve()),'-v','-r',str(a.root.resolve())]
 bad='{z:=0;bad:=1/z;}'
 with tempfile.TemporaryDirectory(prefix='command-chain-boot-',dir=a.root.resolve())as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE']);provider=w/'library.dis64';dependency=w/'dependency.dis64';caller=w/'client.dis64';vmp='/'+str(provider.relative_to(a.root.resolve()));vmd='/'+str(dependency.relative_to(a.root.resolve()));vmc='/'+str(caller.relative_to(a.root.resolve()))
  def compile(row,mandatory=True,soft=False):
   (w/'library.m').write_text(row[5]);outer=row[1];client=row[2]
   if mandatory:client=client.replace('if(m==nil)return;','if(m==nil)'+bad)
   if soft:outer=outer.replace('if(m==nil)'+bad,'if(m==nil)return;')
   for source,name,flag,target in [(row[4],'dependency','-64b',dependency),(outer.replace('"dependency.dis64"','"'+vmd+'"'),'command','-64b',provider),(client.replace('"library.dis64"','"'+vmp+'"'),'client','-64c',caller)]:
    (w/(name+'.b')).write_text(source);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(target),str(w/(name+'.b'))],env=env,capture_output=True,text=True);assert not r.returncode,(row[0],r.stderr)
  def execute(jit,owner='Client',error=''):
   output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmc]);assert 'native64 scalar module '+owner+': jit='+str(jit)+', exit="'+error+'"'in output and BOOT['RETIRED']in output and 'panic:'not in output and 'faults:'not in output and ('Broken:'in output)==bool(error),output
  rows=list(CASES['cases']())
  for row in rows:
   compile(row)
   for jit in (0,1):execute(jit,'Library'if row[0]=='chain-leaf-error'else'Command'if row[3]else'Client','zero divide'if row[3]else'');runs+=1
   # Outer LOAD itself needs no eager dependency construction or init call.
   compile(row,False);provider.unlink()
   for jit in (0,1):execute(jit);runs+=1
   print('PASS full OS importing Command chain:',row[0],'C/RX + missing outer',flush=True)
  row=next(r for r in rows if r[0]=='chain-scalar');compile(row,soft=True);saved=dependency.read_bytes();dependency.unlink()
  for jit in (0,1):execute(jit)
  dependency.write_bytes(saved)
  # A wrong result reaches the real caller guard; a wrong signature must not enter failing code.
  changed=list(row);changed[4]=changed[4].replace('return n+1;','return n+2;');compile(changed)
  for jit in (0,1):execute(jit,'Command','zero divide')
  changed[4]='implement Library;Library:module{inc:fn(n:big):big;};inc(n:big):big{z:=0;bad:=1/z;return n;}'
  compile(changed,soft=True)
  for jit in (0,1):execute(jit)
  row=next(r for r in rows if r[0]=='chain-error');compile(row,False);data=bytearray(provider.read_bytes())
  for i in range(struct.unpack_from('>I',data,28)[0]):
   kind,flags,at,n=struct.unpack_from('>HHII',data,32+12*i)
   if kind==6:data[at+24]^=1
  provider.write_bytes(data)
  for jit in (0,1):execute(jit)
  # Neither this library nor its dependency is run by loading alone.
  row=next(r for r in rows if r[0]=='chain-load-only');compile(row);dependency.unlink()
  for jit in (0,1):execute(jit)
  for path,flags in [(vmc,[]),(vmp,['-X','dis64-calls'])]:
   output=BOOT['execute'](command+flags+['-d',path]);assert 'panic: loading 'in output and BOOT['RETIRED']not in output,output
  en=re.findall(r'```limbo\n(.*?)```',(ROOT/'doc/dis64-command-import.md').read_text(),re.S);ru=re.findall(r'```limbo\n(.*?)```',(ROOT/'doc/dis64-command-import.ru.md').read_text(),re.S);assert len(en)==4 and en==ru
  compile(('tutorial',en[2],en[3],0,en[1],en[0]))
  for jit in (0,1):execute(jit)
  print('PASS importing Command RU/EN tutorial: C/RX',flush=True)
  print('PASS full OS nested signature/result/missing dependency/hash/no-auto-init/policy controls',flush=True)
  print('PASS importing Command full OS:',runs,'main C/RX roots + nested refusal controls',flush=True)
if __name__=='__main__':main()
