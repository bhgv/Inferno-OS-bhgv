#!/usr/bin/env python3
"""Reject forged native index temporaries and entries into borrowed-address pairs."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));C=P['C'];CMD=P['CMD'];W=P['W']
def enc(row,**changes):
 name,mode,source,middle,dest,secondary,_=row
 name=changes.get('name',name);mode=changes.get('mode',mode);source=changes.get('source',source);middle=changes.get('middle',middle);dest=changes.get('dest',dest);secondary=changes.get('secondary',secondary)
 if mode>>3&7==5:
  return bytes([C.OP.CODES[name],mode])+(C.OP.operand(middle)if mode&192 else b'')+C.OP.operand(source)+C.OP.operand(secondary)+C.OP.operand(dest)
 return P['enc'](row,**changes)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('scratch','boundary'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='client-index-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('read','write-word','pair-loop'):
   _,provider,caller,_=next(r for r in CMD['CASES']['indexcases']()if r[0]=='index-'+case)
   for owner,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
    (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
    base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);label=case+'-'+owner;rows.append((label+'-valid',P['rebuild'](base),owner,1))
    def add(name,changes):
     b=dict(base);items=[r[-1]for r in code]
     for pc,fields in changes:items[pc]=enc(code[pc],**fields)
     b[2]=W(len(items))+b''.join(items);rows.append((label+'-'+name,P['rebuild'](b),owner,0))
    idx=next(i for i,r in enumerate(code)if r[0]=='IINDB');ins=code[idx];use=code[idx+1];scratch=ins[3];read=(use[1]>>3&7)==5;field='source'if read else'dest'
    for name,offset in [('header',40),('root',ins[2]),('result',scratch+8),('outside',scratch+16),('unaligned',scratch+1)]:
     add('scratch-'+name,[(idx,dict(middle=offset)),(idx+1,{field:offset})])
    for name,fields in [('owner-context',dict(source=64 if owner=='library'else 0)),('owner-mp',dict(mode=ins[1]&~56)),('middle-immediate',dict(mode=(ins[1]&63)|64)),('index-mp',dict(mode=ins[1]&~7,dest=8)),('index-root',dict(mode=(ins[1]&~7)|1,dest=ins[2])),('index-word-op',dict(name='IINDW')),('index-generic-op',dict(name='IINDX'))]:add(name,[(idx,fields)])
    add('consumer-other-slot',[(idx+1,{field:scratch+8})]);add('consumer-field',[(idx+1,dict(secondary=1))]);add('consumer-word',[(idx+1,dict(name='IMOVW'))]);add('missing-index',[(idx,dict(name='IJMP',mode=26,middle=0,source=0,dest=idx+1))])
    if read:add('result-root',[(idx+1,dict(dest=ins[2]))])
    else:add('source-root',[(idx+1,dict(mode=13,source=ins[2]))])
    branches=[i for i,r in enumerate(code)if r[0]in('IJMP','IBEQW','IBNEW','IBGEW','IBLTW','IBEQL','IBNEL')]
    if branches:add('consumer-entry',[(branches[0],dict(dest=idx+1))])
  if a.control:
   selected='read-library-scratch-result'if a.control=='scratch'else'read-library-consumer-entry';rows=[r for r in rows if r[0]==selected];assert rows
   if a.control=='scratch':
    path='dis64-client-index.h';text=(ROOT/'include'/path).read_text();old='index->middle!=scratch';assert text.count(old)==1;text=text.replace(old,'0');text=text.replace('long scratch=v->frame-16;','long scratch=v->decoded[pc].instruction.middle;')
   else:
    path='dis64-scalar-client.h';text=(ROOT/'include'/path).read_text();old='node->next=i+2;i++;continue;';assert text.count(old)==1;text=text.replace(old,'node->next=i+2;nodes[i+1].boundary=1;nodes[i+1].next=i+2;nodes[i+1].target=v->count;i++;continue;')
   (w/path).write_text(text)
   if path!='dis64-scalar-client.h':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,owner,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),owner,str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS byte index raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
