#!/usr/bin/env python3
"""Independent malformed ordinary-init images and pure matcher omission controls."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-proof.py'));CMD=runpy.run_path(str(ROOT/'utils/tests/dis64-command.py'));CASES=CMD['CASES'];enc=P['instruction'];W=P['W'];C=P['C']
def rebuild(parts):
 at=92;directory=b'';payload=b''
 for kind,data in sorted(parts.items()):directory+=struct.pack('>HHII',kind,1,at,len(data));payload+=data;at+=len(data)
 return b'Dis64\r\n\x1a'+W(0x10000,92,1,0,0,5)+directory+payload

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('hash','class','arguments','export'));a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='command-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE']);rows=[]
  for case in ('consume-two','source-context'):
   _,provider,caller,_=next(r for r in CASES['cases']()if r[0]==case)
   for name,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
    (w/(name+'.b')).write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/(case+'-'+name+'.bin')),str(w/(name+'.b'))],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
  library=(w/'consume-two-library.bin').read_bytes();client=(w/'consume-two-client.bin').read_bytes();parts=CMD['sections'](client);libparts=CMD['sections'](library);decoded=C.decode(parts[2],managed=True);libcode=C.decode(libparts[2]);begin=next(i for i,r in enumerate(decoded)if r[0]=='IMFRAME');string=next(r[2]for r in decoded if r[0]=='ICONSP');argv=decoded[begin+2][2]
  rows += [('valid-client',client,'client',1),('valid-library',library,'library',1),('signature-match',library,'match',1)]
  def codeimage(base,code,pc,**changes):
   p=dict(base);items=[r[-1]for r in code];items[pc]=enc(code[pc],**changes);p[2]=W(len(items))+b''.join(items);return rebuild(p)
  for label,pc,changes in [('context-string',begin+1,dict(mode=13,source=string)),('argv-string',begin+2,dict(source=string)),('context-module',begin+1,dict(source=0)),('argument-offset',begin+1,dict(secondary=68)),('argv-offset',begin+2,dict(secondary=64)),('argument-frame',begin+2,dict(dest=48)),('argument-immediate',begin+1,dict(mode=21,source=7)),('argument-word',begin+1,dict(name='IMOVW',mode=21,source=7)),('call-ordinal',begin+3,dict(middle=1)),('call-module',begin+3,dict(dest=8)),('call-frame',begin+3,dict(source=48))]:rows.append((label,codeimage(parts,decoded,pc,**changes),'client',0))
  branch=next(i for i,r in enumerate(decoded)if r[0]in('IBEQL','IBNEL'))
  for j in range(1,4):rows.append(('island-'+str(j),codeimage(parts,decoded,branch,dest=begin+j),'client',0))
  for name,base,kind,offset,value in [('request-count',parts,5,0,2),('request-module',parts,5,4,0),('request-symbol',parts,5,8,2),('request-version',parts,5,16,1),('request-arity',parts,5,24,1),('request-result',parts,5,28,1),('export-entry',libparts,6,8,1),('export-frame',libparts,6,12,1),('export-version',libparts,6,20,1),('export-arity',libparts,6,28,1),('export-result',libparts,6,32,1)]:
   p=dict(base);v=bytearray(p[kind]);struct.pack_into('>I',v,offset,value);p[kind]=bytes(v);rows.append((name,rebuild(p),'client'if kind==5 else'library',0))
  for label,mask in [('context-map',0x80),('argv-map',0x40)]:
   p=dict(libparts);t=bytearray(p[3]);t[13]&=~mask;p[3]=bytes(t);rows.append((label,rebuild(p),'library',0))
  for label,pc,changes in [('context-as-list',0,dict(name='ILENL',mode=9,source=64,dest=88)),('exit-in-library',len(libcode)-1,dict(name='IEXIT')),('word-context-nil',0,dict(name='IBEQW'if libcode[0][0]=='IBEQL'else'IBNEW'))]:rows.append((label,codeimage(libparts,libcode,pc,**changes),'library',0))
  sourceparts=CMD['sections']((w/'source-context-client.bin').read_bytes());newcode=C.decode(sourceparts[2],managed=True);new=next(i for i,r in enumerate(newcode)if r[0]in('INEW','INEWZ'))
  rows += [('source-context-valid',rebuild(sourceparts),'client',1),('context-descriptor',codeimage(sourceparts,newcode,new,source=0),'client',0),('context-new-mode',codeimage(sourceparts,newcode,new,mode=9),'client',0),('context-newz',codeimage(sourceparts,newcode,new,name='INEWZ'),'client',1)]
  p=dict(libparts);v=bytearray(p[6]);v[24]^=1;p[6]=bytes(v);rows.append(('hash-mismatch',rebuild(p),'match',0));rows.append(('new-hash-still-structural',rebuild(p),'library',1))
  rows += [('truncated-client',client[:-1],'client',0),('trailer-library',library+b'\0','library',0)]
  if a.control:
   path={'hash':'dis64-command-image.h','class':'dis64-client-managed.h','arguments':'dis64-scalar-client.h','export':'dis64-command-binding.h'}[a.control];s=(ROOT/'include'/path).read_text();old,newtext={
    'hash':('return !memcmp(a.signature.data,b.signature.data,a.signature.bytes);','return 1;'),
    'class':('(actual!=Dis64ClientRoot && kind!=Dis64ClientRoot && actual!=kind && !dis64clientmergekind(actual,kind))','0'),
    'arguments':('d->secondary!=64+8*j','0'),
    'export':('(b.entry || b.frame)','0')}[a.control];assert s.count(old)==1;(w/path).write_text(s.replace(old,newtext));
   if path!='dis64-scalar-client.h':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
   if a.control=='export':(w/'dis64-command-image.h').write_text((ROOT/'include/dis64-command-image.h').read_text())
   chosen={'hash':'hash-mismatch','class':'argv-string','arguments':'argument-offset','export':'export-frame'}[a.control];rows=[r for r in rows if r[0]==chosen]
  binary=w/'check';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,mode,expected in rows:
   f=w/(name+'.bin');f.write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(f),mode,str(expected)]+([str(w/'consume-two-client.bin')]if mode=='match'else[]),env=env,capture_output=True,text=True)
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
   if a.control:assert r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr,(name,r.returncode,r.stderr)
   else:assert not r.returncode,(name,r.returncode,r.stderr)
  print('PASS ordinary init raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' independent images/matches',flush=True)
if __name__=='__main__':main()
