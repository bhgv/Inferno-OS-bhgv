#!/usr/bin/env python3
"""Independent scalar importing-image mutations; malformed code never executes."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CLIENT=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'));S=CLIENT['S'];C=S['CALLS'];W=C.words

def rebuild(parts):
 at=92;directory=b'';payload=b''
 for kind in (1,2,3,5,7):
  data=parts[kind];directory+=struct.pack('>HHII',kind,1,at,len(data));payload+=data;at+=len(data)
 return b'Dis64\r\n\x1a'+W(0x10000,92,1,0,0,5)+directory+payload

def instruction(row,**change):
 name,mode,source,middle,dest,secondary,_=row;name=change.get('name',name);mode=change.get('mode',mode);source=change.get('source',source);middle=change.get('middle',middle);dest=change.get('dest',dest);secondary=change.get('secondary',secondary)
 out=bytes([C.OP.CODES[name],mode]);sm=mode>>3&7;dm=mode&7
 if dm==5:return out+C.OP.operand(source)+C.OP.operand(dest)+C.OP.operand(secondary)
 if mode&192:out+=C.OP.operand(middle)
 if sm!=3:out+=C.OP.operand(source)
 if dm!=3:out+=C.OP.operand(dest)
 return out

def mutations(data):
 p=CLIENT['sections'](data);rows=C.decode(p[2]);first=lambda op:next(i for i,r in enumerate(rows)if r[0]==op)
 for label,pc,changes in [
  ('wide-middle',first('IMOVW'),{'name':'IADDW','mode':81,'source':1,'middle':100000}),  ('load-table',first('ILOAD'),{'middle':1}),('load-source',first('ILOAD'),{'source':8}),('load-destination',first('ILOAD'),{'dest':8}),
  ('frame-module',first('IMFRAME'),{'source':8}),('frame-cell',first('IMFRAME'),{'dest':48}),('frame-ordinal',first('IMFRAME'),{'middle':63}),
  ('argument-offset',first('IMFRAME')+1,{'secondary':72}),('argument-frame',first('IMFRAME')+1,{'dest':48}),('undefined-argument',first('IMFRAME')+1,{'mode':13,'source':64}),
  ('result-tail',first('ILEA'),{'source':64}),('result-cell',first('ILEA'),{'secondary':40}),
  ('call-ordinal',first('IMCALL'),{'middle':63}),('call-module',first('IMCALL'),{'dest':8}),
  ('call-frame',first('IMCALL'),{'source':48}),('result-copy',first('IMCALL')+1,{'source':64}),
  ('early-exit',first('IMFRAME'),{'name':'IEXIT','mode':27}),('branch',first('IMFRAME'),{'name':'IJMP','mode':26,'dest':0}),
  ('no-load',first('ILOAD'),{'name':'IMOVW','mode':17,'source':0,'dest':64})]:
  items=[r[-1]for r in rows];items[pc]=instruction(rows[pc],**changes);parts=dict(p);parts[2]=W(len(items))+b''.join(items);yield label,rebuild(parts)
 for label,kind,at,value in [('frame-map',3,12,128),('mp-map',3,-1,192),('profile',1,4,0),('module-data',1,8,16),('signature-result',5,20,3)]:
  parts=dict(p);raw=bytearray(parts[kind]);
  if 'map'in label:raw[at]=value
  else:struct.pack_into('>I',raw,at,value)
  parts[kind]=bytes(raw);yield label,rebuild(parts)
 parts=dict(p);raw=bytearray(parts[7]);# String1 is the path, following the Client name.
 path=4+4+4*len('Client');struct.pack_into('>I',raw,path+4,ord('$'));parts[7]=bytes(raw);yield 'builtin-path',rebuild(parts)
 yield 'truncated',data[:-1];yield 'trailer',data+b'\0'

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('initialized','result','map'));a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='scalar-client-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'library.m').write_text('Library:module{id:fn(n:int):int;};');(w/'client.b').write_text(CLIENT['client']('id',1,[1],[7]));image=w/'client.dis64'
  subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(image),str(w/'client.b')],env=env,check=True)
  cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
  if a.control:
   text=(ROOT/'include/dis64-scalar-client.h').read_text();old,new={'initialized':('if(!write || *count==Dis64ClientSlots)','if(*count==Dis64ClientSlots)'), 'result':('d->secondary!=32','0'), 'map':('if(frame.map[i])return 0;','if(0)return 0;')}[a.control];assert text.count(old)==1;(w/'dis64-scalar-client.h').write_text(text.replace(old,new))
  binary=w/'proof';subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-scalar-client-proof.c'),'-o',str(binary)],env=env,check=True);runner=shlex.split(env.get('NATIVE64_RUNNER',''));rows=[('valid',image.read_bytes(),1)]+[(name,data,0)for name,data in mutations(image.read_bytes())]
  if a.control:rows=[r for r in rows if r[0]=={'initialized':'undefined-argument','result':'result-cell','map':'frame-map'}[a.control]]
  for name,data,expected in rows:
   f=w/(name+'.bin');f.write_bytes(data);r=subprocess.run(runner+[str(binary),str(f),str(expected)],env=env,capture_output=True,text=True)
   if a.control:assert r.returncode==10 and 'client raw proof mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   else:assert not r.returncode,(name,r.returncode,r.stderr)
  print('PASS scalar client raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' immutable inputs',flush=True)
if __name__=='__main__':main()
