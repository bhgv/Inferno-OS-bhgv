#!/usr/bin/env python3
"""Independent codec input/output classes, numeric widths, root maps and modes."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));C=P['C'];CMD=P['CMD'];CASES=CMD['CASES'];W=P['W'];enc=P['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('classes',));a=p.parse_args();env=dict(os.environ);rows=[];seen=set()
 with tempfile.TemporaryDirectory(prefix='client-codecs-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE'])
  for case,op in [('word','ICVTCW'),('big','ICVTCL'),('real','ICVTCF'),('format-word','ICVTWC'),('format-big','ICVTLC'),('format-real','ICVTFC'),('bytes-ascii','ICVTCA'),('bytes-ascii','ICVTAC'),('bytes-ascii','ILENA')]:
   _,provider,caller,_=next(r for r in CASES['codeccases']()if r[0]=='codecs-'+case)
   for owner,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
    (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
    base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);matches=[i for i,r in enumerate(code)if r[0]==op]
    if not matches:continue
    idx=matches[0];ins=code[idx];seen.add(op);label=op+'-'+owner
    rows.append((label+'-valid',P['rebuild'](base),owner,1))
    variants=[('source-context',dict(mode=(ins[1]&~56)|8,source=64 if owner=='library'else 0)),('source-list',dict(mode=(ins[1]&~56)|8,source=72 if owner=='library'else 0)),('destination-context',dict(dest=64 if owner=='library'else 0)),('destination-misaligned',dict(dest=ins[4]+1)),('destination-end',dict(dest=4088)),('destination-mp',dict(mode=ins[1]&~7,dest=8)),('middle',dict(mode=ins[1]|64,middle=0)),('source-misaligned',dict(mode=(ins[1]&~56)|8,source=ins[2]+1))]
    if op in('ICVTWC','ICVTLC','ICVTFC'):
     variants.append(('source-mp',dict(mode=ins[1]&~56,source=24)))
     if owner=='library':variants.append(('source-undefined',dict(mode=(ins[1]&~56)|8,source=next(r[4]for r in code[idx+1:]if r[0]=='IDIVW'))))
    if op in('ICVTLC','ICVTFC'):variants.append(('wide-immediate',dict(mode=(ins[1]&~56)|16,source=1)))
    if op in('ICVTCL','ICVTCF','ICVTLC','ICVTFC'):variants.append(('equal-width-kind',dict(name={'ICVTCL':'ICVTCF','ICVTCF':'ICVTCL','ICVTLC':'ICVTFC','ICVTFC':'ICVTLC'}[op])))
    for name,changes in variants:
     b=dict(base);items=[r[-1]for r in code];items[idx]=enc(ins,**changes);b[2]=W(len(items))+b''.join(items);rows.append((label+'-'+name,P['rebuild'](b),owner,0))
  assert len(seen)==9,seen
  if a.control:
   text=(ROOT/'include/dis64-client-managed.h').read_text();assert text.count('actual!=kind')==1;(w/'dis64-client-managed.h').write_text(text.replace('actual!=kind','0'));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text());rows=[r for r in rows if r[0]=='ICVTAC-library-source-context'];assert rows
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,owner,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),owner,str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS codec raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images /9 operations',flush=True)
if __name__=='__main__':main()
