#!/usr/bin/env python3
"""Derive local scalar results from stores, not binder widths or wire assertions."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'))
def enc(row,**changes):
 name,mode,source,middle,dest,secondary,_=row
 name=changes.get('name',name);mode=changes.get('mode',mode);source=changes.get('source',source);middle=changes.get('middle',middle);dest=changes.get('dest',dest);secondary=changes.get('secondary',secondary)
 if mode&7==5:
  return bytes([C.OP.CODES[name],mode])+(C.OP.operand(middle)if mode&192 else b'')+C.OP.operand(source)+C.OP.operand(dest)+C.OP.operand(secondary)
 return P['enc'](row,**changes)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('defined','kind','merge'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='command-scalar-result-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('int-branch','byte','int','big','real','byte-nested','int-nested','big-nested','real-nested','int-computed','big-computed','real-computed','int-to-byte','byte-to-int','int-to-big','int-to-real','mixed-branches'):
   if case=='mixed-branches':
    text='implement Command;include "command.m";first:fn(x:int,y:real,go:int):int;init(ctxt:ref Draw->Context,argv:list of string){n:=7;r:=real n;v:=first(n,r,1);if(v!=7){z:=0;bad:=1/z;}}first(x:int,y:real,go:int):int{if(go)return x;return x;}'
   else:_,text,_,_=next(r for r in CMD['CASES']['scalarresultcases']()if r[0]=='scalarresult-'+case)
   (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);types=T['types'](base[3]);rows.append((case+'-valid',P['rebuild'](base),1))
   def add(name,changes):
    b=dict(base);items=[r[-1]for r in code]
    for pc,fields in changes:items[pc]=enc(code[pc],**fields)
    b[2]=W(len(items))+b''.join(items);rows.append((case+'-'+name,P['rebuild'](b),0))
   binder=next(i for i,r in enumerate(code)if r[0]=='ILEA');dest=code[binder][2]
   for name,offset in [('header',32),('root',64),('tail',types[0][0]-8),('outside',types[0][0])]:add('binder-'+name,[(binder,dict(source=offset))])
   for name,changes in [('frame',dict(dest=48)),('cell',dict(secondary=40)),('inherit',dict(name='IMOVL',source=32))]:add('binder-'+name,[(binder,changes)])
   ts=[t[:]for t in types];v=bytearray(ts[0][2]);bit=dest//8;v[bit//8]|=0x80>>(bit%8);ts[0][2]=bytes(v);b=dict(base);b[3]=T['typebytes'](ts);rows.append((case+'-binder-mapped',P['rebuild'](b),0))
   stores=[i for i,r in enumerate(code)if r[1]&7==5 and r[4]==32 and r[0]!='IMOVL']
   # IMOVL may also be a real big store; forwarding is identified by source FP32.
   stores += [i for i,r in enumerate(code)if r[0]=='IMOVL'and r[1]&7==5 and r[4]==32 and r[2]!=32]
   for n,pc in enumerate(stores):
    for name,changes in [('header-read',dict(mode=13,source=32,middle=0)),('uninitialized',dict(mode=13,source=types[-1][0]-16,middle=0)),('offset',dict(secondary=1)),('other-cell',dict(dest=48)),('missing',dict(name='IJMP',mode=26,middle=0,source=0,dest=pc+1))]:add('store'+str(n)+'-'+name,[(pc,changes)])
    if code[pc][0]in('IMOVL','IMOVF'):add('store'+str(n)+'-kind',[(pc,dict(name='IMOVF'if code[pc][0]=='IMOVL'else'IMOVL'))])
    if code[pc][1]&192:
     for name,changes in [('no-middle',dict(mode=code[pc][1]&63)),('mp-middle',dict(mode=(code[pc][1]&63)|192)),('header-middle',dict(mode=(code[pc][1]&63)|128,middle=32)),('wide-middle',dict(mode=(code[pc][1]&63)|64,middle=32768))]:add('computed-'+name,[(pc,changes)])
   if case=='mixed-branches':
    # x:int FP64, y:real FP72, go:int FP80: both source spans are valid.
    add('conflicting-results',[(stores[-1],dict(name='IMOVF',mode=13,source=72))])
  if a.control:
   selected={'defined':'int-branch-store0-missing','kind':'big-store0-kind','merge':'mixed-branches-conflicting-results'}[a.control];rows=[r for r in rows if r[0]==selected];assert rows
   path='dis64-client-local.h'if a.control=='merge'else'dis64-scalar-client.h';s=(ROOT/'include'/path).read_text();old,new={'defined':('node->needload=library && v->returning;','node->needload=0;'),'kind':('slots[i].kind!=kind','0'),'merge':('a==b?a:0','a')}[a.control];assert s.count(old)==1;(w/path).write_text(s.replace(old,new))
   if path!='dis64-scalar-client.h':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),'library',str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS scalar result raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
