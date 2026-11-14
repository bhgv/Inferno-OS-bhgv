#!/usr/bin/env python3
"""Run source importing commands in the complete console and real namespace/GC."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CLIENT=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'));BOOT=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('limbo','emu','root'):p.add_argument('--'+name,required=True,type=Path)
 p.add_argument('--runner',default='');p.add_argument('--cfg',action='store_true');p.add_argument('--numeric',action='store_true');p.add_argument('--managed',action='store_true');a=p.parse_args();env=dict(os.environ);command=shlex.split(a.runner)+[str(a.emu.resolve()),'-v','-r',str(a.root.resolve())];runs=0
 with tempfile.TemporaryDirectory(prefix='scalar-client-boot-',dir=a.root.resolve())as td:
  w=Path(td);env['TMPDIR']=str(w);provider=w/'library.dis64';caller=w/'client.dis64';vmprovider='/'+str(provider.relative_to(a.root.resolve()));vmcaller='/'+str(caller.relative_to(a.root.resolve()))
  rows=runpy.run_path(str(ROOT/('utils/tests/dis64-scalar-client-managed-cases.py'if a.managed else'utils/tests/dis64-scalar-client-numeric-cases.py'if a.numeric else'utils/tests/dis64-scalar-client-cases.py')))['cases']()if a.cfg or a.numeric or a.managed else(row+(False,)for row in CLIENT['cases']())
  if a.managed:rows=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-managed-cases.py'))['cases'](len(vmprovider))
  for name,text,fn,kind,args,values,expected,failure,callertext,guarded in rows:
   (w/'library.m').write_text(CLIENT['NS']['declarations'](text));(w/'library.b').write_text(text);(w/'client.b').write_text(callertext.replace('"library.dis64"','"'+vmprovider+'"'))
   for flag,source,target in [('-64b','library.b',provider),('-64c','client.b',caller)]:subprocess.run([str(a.limbo.resolve()),flag,'-I',str(w),'-o',str(target),str(w/source)],env=env,check=True,capture_output=True)
   for jit in (0,1):
    output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmcaller]);error='dereference of nil'if failure==3 else'invalid argument'if failure==2 else'zero divide'if failure else''
    assert 'native64 scalar module '+('Library'if failure==1 else'Client')+': jit='+str(jit)+', exit="'+error+'"'in output and BOOT['RETIRED']in output and 'panic:'not in output and 'faults:'not in output and ('Broken:'in output)==bool(failure),output
    runs+=1;print('PASS full OS importing Limbo client:',name,'jit='+str(jit),'error='+repr(error),flush=True)
   if a.cfg or a.numeric or a.managed:
    saved=provider.read_bytes();provider.unlink()
    for jit in (0,1):
     output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmcaller]);assert 'native64 scalar module Client: jit='+str(jit)+', exit=""'in output and BOOT['RETIRED']in output and 'Broken:'not in output and 'panic:'not in output and 'faults:'not in output,output
     runs+=1
    provider.write_bytes(saved)
  if a.cfg or a.numeric or a.managed:
   if a.numeric or a.managed:
    (w/'library.m').write_text('Library:module{inc:fn(n:int):int;};')
    (w/'library.b').write_text('implement Library;Library:module{inc:fn(n:int):int;};inc(n:int):int{return n+1;}')
    (w/'client.b').write_text(next(iter(runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-numeric-cases.py'))['cases']()))[8].replace('"library.dis64"','"'+vmprovider+'"'))
    subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(caller),str(w/'client.b')],env=env,check=True,capture_output=True)
   wrong=(w/'library.b').read_text().replace('return n+1;','return n+2;');assert wrong!=(w/'library.b').read_text();(w/'library.b').write_text(wrong)
   subprocess.run([str(a.limbo.resolve()),'-64b','-o',str(provider),str(w/'library.b')],env=env,check=True,capture_output=True)
   for jit in (0,1):
    output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmcaller]);assert 'native64 scalar module Client:'in output and 'exit="zero divide"'in output and BOOT['RETIRED']in output and 'panic:'not in output and 'faults:'not in output,output
   output=BOOT['execute'](command+['-c','1','-d',vmcaller]);assert 'panic: loading 'in output and BOOT['RETIRED']not in output,output
   print('PASS '+('managed'if a.managed else'numeric'if a.numeric else'CFG')+' wrong-result conditional controls: C/RX',flush=True)
   print('PASS full OS '+('managed'if a.managed else'numeric'if a.numeric else'CFG')+' clients including graceful missing-file returns:',runs,'C/RX OS roots',flush=True);return

  saved=provider.read_bytes()
  # Validly typed wrong implementation must trip the command's own integer result guard.
  wrong=(w/'library.b').read_text().replace('return n+1;','return n+2;');assert wrong!=(w/'library.b').read_text();(w/'library.b').write_text(wrong)
  subprocess.run([str(a.limbo.resolve()),'-64b','-o',str(provider),str(w/'library.b')],env=env,check=True,capture_output=True)
  for jit in (0,1):
   output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmcaller]);assert 'native64 scalar module Client:'in output and 'exit="zero divide"'in output and BOOT['RETIRED']in output and 'panic:'not in output and 'faults:'not in output,output
  (w/'library.b').write_text('implement Library;Library:module{inc:fn(n:big):big;};inc(n:big):big{return n;}')
  subprocess.run([str(a.limbo.resolve()),'-64b','-o',str(provider),str(w/'library.b')],env=env,check=True,capture_output=True)
  for jit in (0,1):
   output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmcaller]);assert 'exit="module not loaded"'in output and BOOT['RETIRED']in output and 'panic:'not in output and 'faults:'not in output,output
  provider.write_bytes(saved);provider.unlink()
  for jit in (0,1):
   output=BOOT['execute'](command+['-c',str(jit),'-X','dis64-calls','-d',vmcaller]);assert 'exit="module not loaded"'in output and BOOT['RETIRED']in output and 'Broken:'in output and 'panic:'not in output and 'faults:'not in output,output
  provider.write_bytes(saved)
  output=BOOT['execute'](command+['-c','1','-d',vmcaller]);assert 'panic: loading 'in output and BOOT['RETIRED']not in output,output
  for changed in CLIENT['sections'](caller.read_bytes()).keys():
   if changed!=3:continue
   data=bytearray(caller.read_bytes());# First frame map byte is12 bytes into the Types payload.
   import struct
   for i in range(struct.unpack_from('>I',data,28)[0]):
    k,flags,at,n=struct.unpack_from('>HHII',data,32+12*i)
    if k==3:data[at+12]=128
   caller.write_bytes(data)
  output=BOOT['execute'](command+['-X','dis64-calls','-d',vmcaller]);assert 'panic: loading 'in output and BOOT['RETIRED']not in output,output
  print('PASS full OS wrong-result controls, signature mismatch, missing-provider retirement and malformed-client refusal',flush=True)
 print('PASS full OS scalar file-client pipeline:',runs,'C/RX OS roots; no native-authored importing CODE',flush=True)
if __name__=='__main__':main()
