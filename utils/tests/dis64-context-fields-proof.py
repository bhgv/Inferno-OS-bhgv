#!/usr/bin/env python3
"""Independent Context field framing, map and opaque-role refusal checks."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));C=P['C'];W=P['W'];CMD=P['CMD'];CASES=CMD['CASES'];OP=C.OP

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('offset','owner','role'));a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='context-fields-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE']);parts={}
  _,provider,_,_=next(r for r in CASES['fieldcases']()if r[0]=='fields-restore');_,_,caller,_=next(r for r in CASES['fieldcases']()if r[0]=='fields-caller')
  for name,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
   (w/(name+'.b')).write_text(text);subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/(name+'.bin')),str(w/(name+'.b'))],env=env,check=True,capture_output=True);parts[name]=CMD['sections']((w/(name+'.bin')).read_bytes())
  rows=[]
  def add(label,base,pc=None,**changes):
   code=C.decode(base[2],managed=True);items=[r[-1]for r in code]
   if pc is not None:
    name,mode,source,middle,dest,secondary,_=code[pc];name=changes.get('name',name);mode=changes.get('mode',mode);source=changes.get('source',source);dest=changes.get('dest',dest);secondary=changes.get('secondary',secondary)
    if mode>>3&7==5:raw=bytes((OP.CODES[name],mode))+OP.operand(source)+OP.operand(secondary)+OP.operand(dest)
    elif mode&7==5:raw=bytes((OP.CODES[name],mode))+OP.operand(source)+OP.operand(dest)+OP.operand(secondary)
    else:raw=bytes((OP.CODES[name],mode))+OP.operand(source)+OP.operand(dest)
    items[pc]=raw
   out=dict(base);out[2]=W(len(items))+b''.join(items);rows.append((label,P['rebuild'](out),'library'if base is parts['library']else'client',pc is None))
  for owner,base in parts.items():
   add(owner+'-valid',base);code=C.decode(base[2],managed=True);reads=[i for i,r in enumerate(code)if r[1]>>3&7==5];writes=[i for i,r in enumerate(code)if r[1]&7==5 and r[5]<24];assert len(reads)==3 and len(writes)==6,(owner,reads,writes)
   r=reads[0];d=code[r][4];root=code[r][2]
   for n in (-8,4,24):add(owner+'-read-offset-'+str(n),base,r,secondary=n)
   for n in (-8,4,24,64):add(owner+'-write-offset-'+str(n),base,writes[0],secondary=n)
   for label,pc,changes in [('read-prefix',r,dict(source=40)),('read-argv',r,dict(source=72)),('read-mp',r,dict(mode=33)),('read-tail',r,dict(dest=int.from_bytes(base[3][4:8],'big')-8)),('read-numeric',r,dict(name='IMOVW')),('write-argv',writes[0],dict(dest=72)),('write-immediate',writes[0],dict(mode=21,source=1)),('write-path',writes[0],dict(mode=5,source=16)),('write-list',writes[-1],dict(source=72)),('display-to-screen',writes[-2],dict(source=d))]:add(owner+'-'+label,base,pc,**changes)
   # One mapped root bit is omitted; it must refuse before construction.
   types=bytearray(base[3]);types[12+d//64]&=~(0x80>>((d//8)%8));out=dict(base);out[3]=bytes(types);rows.append((owner+'-read-map',P['rebuild'](out),'library'if owner=='library'else'client',False))
  if a.control:
   text=(ROOT/'include/dis64-client-managed.h').read_text();old,new={'offset':('d->secondary%8','d->secondary%4'),'owner':('read?in->source:in->destination,Dis64ClientOpaque','read?in->source:in->destination,Dis64ClientRoot'),'role':('actual!=kind','0')}[a.control]
   assert text.count(old)==1;(w/'dis64-client-managed.h').write_text(text.replace(old,new));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
   chosen={'offset':'library-read-offset-4','owner':'library-read-argv','role':'library-display-to-screen'}[a.control];rows=[r for r in rows if r[0]==chosen]
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,mode,valid in rows:
   f=w/'test.bin';f.write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(f),mode,str(int(valid))],env=env,capture_output=True,text=True)
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
   assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS Context field raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
