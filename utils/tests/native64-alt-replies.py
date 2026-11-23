#!/usr/bin/env python3
"""Execute mixed int/String reply channels and blocking/default ALT selections."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
PAIR=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=PAIR['G'],PAIR['CMD'],PAIR['E']
PREFIX='include "sys.m";include "draw.m";'
PAYLOAD='(int,string)';ARGS='c:chan of '+PAYLOAD+',n:int,s:string,other:chan of '+PAYLOAD
NAMES=('run','reverse','duplicate','mp','plain','miss','ready','readreply','nilreply')
DECL='Replies:module{'+''.join(n+':fn('+ARGS+');'for n in (*NAMES,'refreply'))+'mkpair:fn():chan of '+PAYLOAD+';mkcontext:fn():ref Draw->Context;clear:fn();};'
SOURCE='implement Replies;'+PREFIX+DECL+'shared,alternate:chan of '+PAYLOAD+';sourceword,wordout,received,sent,choice,fallback:int;source,output:string;both:'+PAYLOAD+';rawout:array of byte;'
for name,body in(zip(NAMES,('unused:=mkpair();spawn reader();spawn sender();','spawn sender();spawn readerrev();','spawn readerdup();spawn sender();','both=(n,s);spawn reader();spawn sendermp();','spawn plainreader();spawn plainsender();','v:=(n,s);alt{shared <-=v=>sent++;* =>fallback++;}alt{(a,b):= <-alternate=>wordout=a;output=b;received++;* =>fallback++;}','v:=(n,s);shared <-=v;alt{(a,b):= <-shared=>wordout=a;output=b;received++;choice=1;* =>fallback++;}alt{shared <-=v=>sent++;* =>fallback++;}(a,b):= <-shared;wordout=a;output=b;','d:=array of byte s;off:=0;if(n>0 && len d>0)off=n%len d;readc:=chan[1] of (array of byte,string);alt{readc <-=(d[off:],nil)=>sent++;* =>fallback++;}alt{(v,e):= <-readc=>rawout=v;output=e;received++;choice=1;* =>fallback++;}','replyc:=chan[1] of (int,string);alt{replyc <-=(n,nil)=>sent++;* =>fallback++;}alt{(v,e):= <-replyc=>wordout=v;output=e;received++;choice=1;* =>fallback++;}'))):
 SOURCE+=name+'('+ARGS+'){prepare(c,n,s,other);'+body+'}'
SOURCE+='mkpair():chan of '+PAYLOAD+'{return chan of '+PAYLOAD+';}mkcontext():ref Draw->Context{return ref Draw->Context;}clear(){source=nil;output=nil;}prepare('+ARGS+'){unusedctxt:=mkcontext();shared=c;alternate=other;sourceword=n;source=s;}'
for name,channels in(('reader',('shared','alternate')),('readerrev',('alternate','shared')),('readerdup',('shared','shared'))):
 SOURCE+=name+'(){alt{'+''.join('(a,b):= <-'+c+'=>wordout=a;output=b;received++;choice='+str(i+1)+';'for i,c in enumerate(channels))+'}}'
SOURCE+='sender(){v:=(sourceword,source);alt{shared <-=v=>sent++;}}sendermp(){alt{shared <-=both=>sent++;}}plainreader(){(a,b):= <-shared;wordout=a;output=b;received++;choice=1;}plainsender(){v:=(sourceword,source);shared <-=v;sent++;}'
SOURCE+='''
Packet:adt {name:string;};
originalpacket, receivedroot: ref Packet;
refreply('''+ARGS+'''){
    reply: chan of (int, ref Packet);
    if (n < 0)
        reply = chan of (int, ref Packet);
    else
        reply = chan[1] of (int, ref Packet);
    packet: ref Packet;
    if (len s > 0) packet = ref Packet(s);
    spawn sendref(reply, n, packet);
    (w, receivedpacket) := <-reply;
    originalpacket = packet;
    receivedroot = receivedpacket;
    output = nil;
    if (receivedpacket != nil) output = receivedpacket.name;
    wordout = w;
    sent++;
    received++;
    choice = 1;
    spawn sendref(reply, n, nil);
    <-reply;
}
sendref(reply:chan of (int, ref Packet), n:int, packet:ref Packet){
    value := (n, packet);
    copied := value;
    reply <-= copied;
}
'''
def generate(limbo,w):
 m=w/'replies.m';m.write_text(PREFIX+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Replies','-I',str(ROOT/'module'),str(m)],text=True);signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Replies_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Replies',(22,0,0,'~0U'),export_signatures=signatures);p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};s+='\nstatic const uint reply_globals[]={'+','.join(str(gl[n])for n in('output','wordout','received','sent','choice','fallback','rawout'))+'};\n'
 s+=f'static const uint original_packet={gl["originalpacket"]}, received_packet={gl["receivedroot"]};\n'
 p.write_text(s)
 refreply=next(r for r in rows if r['name']=='refreply')
 sendref=next(r for r in rows if r['name']=='sendref')
 assert {step[5] for step in refreply['code']} >= {136,183}
 assert {step[5] for step in sendref['code']} >= {135,136}
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(134,135,136,191)and not(c[5]in(183,185)and c[2]==1):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,tables,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('reply oracle missed',row['name'],pc,k))
 print('PASS reply independent oracle:',controls,'mutations; exact old descriptors, native fields, channel ABI and default jump tables',flush=True)
 data,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 assert len(records)==20 and all(r['accepted']for r in records),[(r['name'],r.get('pc'),r.get('reason'))for r in records if not r['accepted']]
 for row in records:
  if row['accepted']:CMD['verify_nilbranches'](data,row,tables,rows=records)
 print('PASS all 20 unchanged AWM bodies; remaining refusals:',[(r['name'],r.get('pc'),r.get('reason'))for r in records if not r['accepted']],flush=True)
def unit():
 original=PAIR['unit']();src=original.split('\n#line 1 "native64-alt-pairs.c"')[0];extract=CMD['NS']['S']['FILES']['FILES']['function'];heap=(ROOT/'libinterp/heap.c').read_text();vm=(ROOT/'libinterp/xec.c').read_text()
 for sig in('Heap*heap(','void\nacheck(','Heap*\nheaparray(','void\ninitarray(','void\nfreearray('):
  if sig in src:src=src.replace(extract(src,sig),'')
 src+='\nvoid initarray(Type*,Array*);\n'
 for sig in('Heap*\nheap(','void\nacheck(','Heap*\nheaparray(','void\ninitarray(','void\nfreearray('):src+='\n'+extract(heap,sig)+'\n'
 for sig in('static uchar*\narrayspan(','OP(slicea)','OP(lena)','static void\nnewrecord(','OP(new)','OP(newz)'):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 src+='\n'+extract((ROOT/'libinterp/runt.c').read_text(),'Array*\nmem2array(')+'\n'
 src+='\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(cvtca)')+'\n'
 start=original.rindex(' optab[IMOVW]=countmovw;');end=original.index(' optab[IMOVP]=actual_movp;',start)
 return src+'\n#line 1 "native64-alt-replies.c"\n'+(ROOT/'utils/tests/native64-alt-replies.c').read_text().replace(' REPLY_HANDLERS',original[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='alt-replies-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  original=unit();extract=CMD['NS']['S']['FILES']['FILES']['function'];body=extract(original,'OP(movmp)')
  controls=(
   ('wrong mixed descriptor','native64-typed-module.h','if(mixedreply){uchar map[]={0x40};','if(mixedreply){uchar map[]={0x80};',None,'shared mixed reply descriptor'),
   ('short mixed copy',None,None,None,original.replace(body,body.replace('size = mappedcopytype(t);','size = mappedcopytype(t);if(size==16 && t->map[0]==0x40)size=8;'),1),'exact signed Word32 and String identity'),
   ('blocking default selection','native64-typed-code.h','p->reg=(s->returned==134 || s->returned==190 ||','if(s->op==INBALT)p->op=IALT;p->reg=(s->returned==134 || s->returned==190 ||',None,'all reply peers resumed'))
  parents=('native64-typed-module.h','native64-typed-alt.h','native64-typed-multi.h','native64-typed-code.h')
  for name,header,before,after,source,predicate in controls:
   if header:
    for parent in parents:(w/parent).write_text((ROOT/'include'/parent).read_text())
    text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1))
   (w/'unit.c').write_text(source or original);subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   if header:
    for parent in parents:(w/parent).unlink()
   print('PASS ALT reply active control:',name,flush=True)

if __name__=='__main__':main()
