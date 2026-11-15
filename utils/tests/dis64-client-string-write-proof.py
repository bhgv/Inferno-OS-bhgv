#!/usr/bin/env python3
"""Independent managed String write operand, width and ownership admission."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));C=P['C'];CMD=P['CMD'];CASES=CMD['CASES'];W=P['W'];enc=P['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('classes',));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='string-write-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE'])
  for case,op in [('strings-concat','IADDC'),('strings-cow','IINSC'),('strings-slice','ISLICEC')]:
   _,provider,caller,_=next(r for r in CASES['stringwritecases']()if r[0]==case)
   for owner,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
    (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
    base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);idx=next(i for i,r in enumerate(code)if r[0]==op);ins=code[idx];rows.append((case+'-'+owner+'-valid',P['rebuild'](base),owner,1))
    variants=[('destination-mp',dict(mode=(ins[1]&~7))),('destination-context',dict(dest=64 if owner=='library'else 0)),('destination-misaligned',dict(dest=ins[4]+1)),('destination-end',dict(dest=4088)),('middle-role',dict(mode=(ins[1]&63)|(64 if op=='IADDC'else 192),middle=8)),('source-role',dict(mode=(ins[1]&~56)|8,source=64 if owner=='library'else 0))]
    if op!='IADDC':variants += [('middle-wide',dict(mode=(ins[1]&63)|64,middle=32768)),('source-mp',dict(mode=ins[1]&~56,source=24)),('source-uninitialized',dict(mode=(ins[1]&~56)|8,source=next(r[4]for r in code[idx+1:]if r[0]=='IDIVW')))]
    for label,changes in variants:
     b=dict(base);items=[r[-1]for r in code];items[idx]=enc(ins,**changes);b[2]=W(len(items))+b''.join(items);rows.append((case+'-'+owner+'-'+label,P['rebuild'](b),owner,0))
  if a.control:
   text=(ROOT/'include/dis64-client-managed.h').read_text();assert text.count('actual!=kind')==1;(w/'dis64-client-managed.h').write_text(text.replace('actual!=kind','0'));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text());rows=[r for r in rows if r[0]=='strings-cow-library-destination-context'];assert rows
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,owner,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),owner,str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS String write raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
