#!/usr/bin/env python3
"""Entryless importers keep independent incoming exports and outgoing requests."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));enc=runpy.run_path(str(ROOT/'utils/tests/dis64-command-scalar-result-proof.py'))['enc'];CASES=runpy.run_path(str(ROOT/'utils/tests/dis64-command-chain-cases.py'))
def rebuild(parts):
 at=32+12*len(parts);header=at;directory=b'';payload=b''
 for kind,data in sorted(parts.items()):directory+=struct.pack('>HHII',kind,1,at,len(data));payload+=data;at+=len(data)
 return b'Dis64\r\n\x1a'+W(0x10000,header,1,0,0,len(parts))+directory+payload
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('load','export'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='command-chain-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE'])
  for case in ('scalar','context','branch','loop','helper','before','owned','unicode','array','command'):
   row=next(r for r in CASES['cases']()if r[0]=='chain-'+case);(w/'library.m').write_text(row[5]);(w/'source.b').write_text(row[1]);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   (w/'source.b').write_text(row[2]);r=subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'caller'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   base=CMD['sections']((w/'image').read_bytes());assert set(base)=={1,2,3,5,6,7};code=C.decode(base[2],managed=True);types=T['types'](base[3]);rows.extend([(case+'-valid',rebuild(base),'library',1),(case+'-not-executable',rebuild(base),'client',0),(case+'-match',rebuild(base),'match',1)])
   def add(name,b,mode='library',expected=0):rows.append((case+'-'+name,rebuild(b),mode,expected))
   def mutate(name,changes):
    b=dict(base);items=[r[-1]for r in code]
    for pc,fields in changes:items[pc]=enc(code[pc],**fields)
    b[2]=W(len(items))+b''.join(items);add(name,b)
   for name,key in [('missing-imports',5),('missing-exports',6)]:b=dict(base);del b[key];add(name,b)
   for name,key,offset,value in [('export-count',6,0,2),('export-symbol',6,4,3),('export-entry',6,8,1),('export-frame',6,12,1),('export-result',6,32,1),('export-arity',6,28,1),('import-symbol',5,8,4),('import-module',5,4,0),('module-name',1,0,2)]:
    b=dict(base);data=bytearray(b[key]);struct.pack_into('>I',data,offset,value);b[key]=bytes(data);add(name,b)
   b=dict(base);data=bytearray(b[6]);data[24]^=1;b[6]=bytes(data);add('hash-structural',b,'library',1);add('hash-mismatch',b,'match',0)
   for bit in (8,9):
    ts=[t[:]for t in types];v=bytearray(ts[0][2]);v[bit//8]&=~(0x80>>(bit%8));ts[0][2]=bytes(v);b=dict(base);b[3]=T['typebytes'](ts);add('argument-map'+str(bit),b)
   load=next(i for i,r in enumerate(code)if r[0]=='ILOAD');frame=next(i for i,r in enumerate(code)if r[0]=='IMFRAME');ret=next(i for i in range(len(code)-1,-1,-1)if code[i][0]=='IRET')
   for name,changes in [('missing-load',[(load,dict(name='IJMP',mode=26,source=0,middle=0,dest=load+1))]),('load-module-slot',[(load,dict(dest=8))]),('load-path',[(load,dict(source=24))]),('import-frame',[(frame,dict(dest=48))]),('exit',[(ret,dict(name='IEXIT'))])]:mutate(name,changes)
   branches=[i for i,r in enumerate(code)if r[0]in('IJMP','IBEQW','IBNEW','IBEQL','IBNEL','IBEQC','IBNEC')]
   if branches:mutate('import-setup-entry',[(branches[0],dict(dest=frame+1))])
  if a.control:
   chosen={'load':'scalar-missing-load','export':'scalar-match'}[a.control];rows=[r for r in rows if r[0]==chosen];assert rows
   path='dis64-scalar-client.h'if a.control=='load'else'dis64-command-image.h';text=(ROOT/'include'/path).read_text();old,new={'load':('node->needload=1;ordinal=in->middle;','node->needload=0;ordinal=in->middle;'),'export':('v->importing?&v->exports:&v->imports','&v->imports')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new))
   if path!='dis64-scalar-client.h':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,mode,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),mode,str(expected)]+([str(w/'caller')]if mode=='match'else[]),env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS entryless importing Command raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
