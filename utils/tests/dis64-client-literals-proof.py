#!/usr/bin/env python3
"""Independent literal pool/MP/Context geometry and String opcode admission."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));C=P['C'];CMD=P['CMD'];CASES=CMD['CASES'];W=P['W'];enc=P['enc']
def pool(data):
 at=4;out=[]
 for i in range(int.from_bytes(data[:4],'big')):
  n=int.from_bytes(data[at:at+4],'big');out.append([int.from_bytes(data[j:j+4],'big')for j in range(at+4,at+4+4*n,4)]);at+=4+4*n
 assert at==len(data);return out
def types(data):
 at=4;out=[]
 for i in range(int.from_bytes(data[:4],'big')):
  n=int.from_bytes(data[at:at+4],'big');a=int.from_bytes(data[at+4:at+8],'big');b=(n+63)//64;out.append([n,a,data[at+8:at+8+b]]);at+=8+b
 assert at==len(data);return out
def typebytes(ts):return W(len(ts))+b''.join(W(n,a)+m for n,a,m in ts)
def poolbytes(ps):return W(len(ps))+b''.join(W(len(v),*v)for v in ps)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('unused','descriptor','budget','classes'));a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='client-literals-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE']);parts={};rows=[]
  for case in ('literals-context','literals-unicode','literals-order','literals-total-limit'):
   _,provider,caller,_=next(r for r in CASES['literalcases']()if r[0]==case)
   for name,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
    (w/(name+'.b')).write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image.bin'),str(w/(name+'.b'))],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr;parts[case+'-'+name]=CMD['sections']((w/'image.bin').read_bytes())
  def add(label,base,mode,valid=False):rows.append((label,P['rebuild'](base),mode,int(valid)))
  for owner in ('library','client'):
   base=parts['literals-context-'+owner];add(owner+'-valid',base,owner,True);ts=types(base[3]);ps=pool(base[7]);assert len(ts)==3 and len(ps)>4 and ts[2]==[24,8,b'\xe0']
   for label,index,new in [('mp-map',1,[ts[1][0],8,bytes([ts[1][2][0]^0x10])]),('context-map',2,[24,8,b'\x80']),('context-size',2,[32,8,b'\xe0'])]:
    t=[x[:]for x in ts];t[index]=new;b=dict(base);b[3]=typebytes(t);add(owner+'-'+label,b,owner)
   b=dict(base);b[3]=typebytes(ts[:2]);add(owner+'-missing-context',b,owner)
   b=dict(base);m=bytearray(b[1]);m[8:12]=W(ts[1][0]+8);b[1]=bytes(m);add(owner+'-mp-size',b,owner)
   for label,value,valid in [('empty',[],True),('signed',[0,0x80000000,0xffffffff],True),('wide',[128640,0,0x10ffff],True),('oversize',[65]*256,False)]:
    p0=[x[:]for x in ps];p0[4]=value;b=dict(base);b[7]=poolbytes(p0);add(owner+'-'+label,b,owner,valid)
   # A structurally complete extra root with no corresponding instruction use.
   p0=ps+[[117]];t=[x[:]for x in ts];n=t[1][0]+8;nr=n//8;t[1]=[n,8,bytes(sum(0x80>>j for j in range(8)if i*8+j<nr)for i in range((nr+7)//8))];b=dict(base);m=bytearray(b[1]);m[8:12]=W(n);b[1]=bytes(m);b[3]=typebytes(t);b[7]=poolbytes(p0);add(owner+'-unused',b,owner)
   code=C.decode(base[2],managed=True);new=next(i for i,r in enumerate(code)if r[0]in('INEW','INEWZ'))
   for label,value in [('mp-allocation',1),('frame-allocation',0)]:
    b=dict(base);items=[r[-1]for r in code];items[new]=enc(code[new],source=value);b[2]=W(len(items))+b''.join(items);add(owner+'-'+label,b,owner)
  base=parts['literals-unicode-library'];code=C.decode(base[2],managed=True);idx=next(i for i,r in enumerate(code)if r[0]=='IINDC')
  for label,changes in [('index-root',dict(mode=137,middle=72)),('index-mp',dict(mode=201,middle=24)),('index-context',dict(source=64)),('index-mapped-result',dict(dest=64)),('index-immediate-wide',dict(middle=32768))]:
   b=dict(base);items=[r[-1]for r in code];items[idx]=enc(code[idx],**changes);b[2]=W(len(items))+b''.join(items);add(label,b,'library')
  base=parts['literals-order-library'];code=C.decode(base[2],managed=True);cmp=next(i for i,r in enumerate(code)if r[0]in('IBEQC','IBNEC','IBLTC','IBLEC','IBGTC','IBGEC'))
  for label,changes in [('compare-list',dict(mode=138,middle=72)),('compare-context',dict(source=64)),('compare-immediate',dict(mode=74,middle=1)),('compare-target',dict(dest=len(code)))]:
   b=dict(base);items=[r[-1]for r in code];items[cmp]=enc(code[cmp],**changes);b[2]=W(len(items))+b''.join(items);add(label,b,'library')
  b=dict(parts['literals-total-limit-library']);ps=pool(b[7]);assert sum(map(len,ps[4:]))==1024;ps[-1].append(97);b[7]=poolbytes(ps);add('budget',b,'library')
  if a.control:
   path='dis64-client-managed.h'if a.control=='classes'else'dis64-client-literals.h';text=(ROOT/'include'/path).read_text();old,new={'unused':('seen==expected','1'),'descriptor':('(unsigned long)in->source!=v->contexttype','0'),'budget':('value.count>Dis64ManagedLiteralRunes-total','0'),'classes':('actual!=kind','0')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text());chosen={'unused':'library-unused','descriptor':'library-mp-allocation','budget':'budget','classes':'compare-context'}[a.control];rows=[r for r in rows if r[0]==chosen]
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,mode,expected in rows:
   f=w/'test.bin';f.write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(f),mode,str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS client literal raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
