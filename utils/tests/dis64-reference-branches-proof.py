#!/usr/bin/env python3
"""Check complete native reference comparisons without executing malformed images."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));C=P['C'];CMD=P['CMD'];CASES=CMD['CASES'];OP=C.OP;W=P['W']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('offset','nil','owner','roles','classification'));a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='reference-branches-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE']);parts={};_,provider,_,_=next(r for r in CASES['branchcases']()if r[0]=='branches-fields');_,_,caller,_=next(r for r in CASES['branchcases']()if r[0]=='branches-caller');numeric='seed:=7;wide:=big seed;widecopy:=wide;if(widecopy!=wide){z:=0;bad:=1/z;}'
  caller=caller[:-1]+numeric+'}';provider=provider[:-1]+'alias:=ctxt;if(alias!=ctxt){z:=0;bad:=1/z;}copy:=argv;if(copy!=argv){z:=0;bad:=1/z;}'+numeric+'}'
  for name,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
   (w/(name+'.b')).write_text(text);subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/(name+'.bin')),str(w/(name+'.b'))],env=env,check=True,capture_output=True);parts[name]=CMD['sections']((w/(name+'.bin')).read_bytes())
  rows=[]
  def change(owner,label,pc=None,**changes):
   base=parts[owner];code=C.decode(base[2],managed=True);items=[r[-1]for r in code]
   if pc is not None:
    name,mode,source,middle,dest,secondary,_=code[pc];name=changes.get('name',name);mode=changes.get('mode',mode);source=changes.get('source',source);middle=changes.get('middle',middle);dest=changes.get('dest',dest);secondary=changes.get('secondary',secondary)
    values=([middle]if mode&192 else[])+([source,secondary]if mode>>3&7==5 else[source])+[dest]
    items[pc]=bytes((OP.CODES[name],mode))+b''.join(OP.operand(n)for n in values)
   out=dict(base);out[2]=W(len(items))+b''.join(items);rows.append((owner+'-'+label,P['rebuild'](out),owner,pc is None))
  for owner,base in parts.items():
   code=C.decode(base[2],managed=True);fields=[i for i,r in enumerate(code)if r[1]>>3&7==5];pairs=[i for i,r in enumerate(code)if r[0]in('IBEQL','IBNEL')and r[1]&192==128];assert len(fields)==3 and [code[i][5]for i in fields]==[0,8,16]and pairs,(owner,fields,pairs)
   f=fields[0];pair=pairs[0];change(owner,'valid');argv=72 if owner=='library'else next(r[2]for r in code if r[0]=='IMOVP'and r[1]&7==5 and r[5]==72)
   for n in (-8,4,24):change(owner,'field-offset-'+str(n),f,secondary=n)
   for label,pc,changes in [('field-owner-argv',f,dict(source=argv)),('field-owner-prefix',f,dict(source=40)),('field-nil-path',f,dict(middle=16)),('field-nil-module',f,dict(middle=0)),('field-word',f,dict(name='IBEQW'if code[f][0]=='IBEQL'else'IBNEW')),('field-fp-middle',f,dict(mode=170)),('field-no-middle',f,dict(mode=42)),('field-target-negative',f,dict(dest=-1)),('field-target-end',f,dict(dest=len(code))),('pair-word',pair,dict(name='IBEQW'if code[pair][0]=='IBEQL'else'IBNEW')),('pair-class',pair,dict(middle=argv)),('pair-prefix',pair,dict(middle=40)),('pair-immediate',pair,dict(mode=74,middle=1)),('pair-tail',pair,dict(middle=int.from_bytes(base[3][4:8],'big')-8))]:change(owner,label,pc,**changes)
   t=bytearray(base[3]);offset=code[f][2];t[12+offset//64]&=~(0x80>>((offset//8)%8));out=dict(base);out[3]=bytes(t);rows.append((owner+'-owner-map',P['rebuild'](out),owner,False))
   if owner=='client':
    frame=next(i for i,r in enumerate(code)if r[0]=='IMFRAME')
    for i in (1,2,3):change(owner,'island-'+str(i),f,dest=frame+i)
  if a.control:
   text=(ROOT/'include/dis64-client-managed.h').read_text();old,new={'offset':('d->secondary%8','d->secondary%4'),'nil':('in->middle!=8','0'),'owner':('read?in->source:in->destination,Dis64ClientOpaque','read?in->source:in->destination,Dis64ClientRoot'),'roles':('actual!=kind','0'),'classification':('(dis64clientrootkind(slots,count,AFP,in->source) || dis64clientrootkind(slots,count,AFP,in->middle))','1')}[a.control];assert text.count(old)==1;(w/'dis64-client-managed.h').write_text(text.replace(old,new));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text());chosen={'offset':'library-field-offset-4','nil':'library-field-nil-path','owner':'library-field-owner-argv','roles':'library-pair-class','classification':'library-valid'}[a.control];rows=[r for r in rows if r[0]==chosen]
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,mode,expected in rows:
   file=w/'test.bin';file.write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(file),mode,str(int(expected))],env=env,capture_output=True,text=True)
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
   assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS native reference raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
