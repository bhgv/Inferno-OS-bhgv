#!/usr/bin/env python3
"""Execute unchanged checked emuinit in real emulators, OS namespace and Sys."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BOOT=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
def provider(limbo,work,counts,fault=False,classic=False):
 source='implement Command;include "sys.m";include "draw.m";include "sh.m";init(ctxt:ref Draw->Context,argv:list of string){z:=0;'
 source+='if(ctxt!=nil || len argv!='+str(len(counts))+')z/=z;'
 for i,count in enumerate(counts):
  source+='s'+str(i)+':=hd argv;if(len s'+str(i)+'!='+str(count)+')z/=z;argv=tl argv;'
 if fault:source+='z/=z;'
 source+='}'
 b=work/'command.b';b.write_text(source);image=work/'command.dis64';command=[str(limbo)]+([]if classic else['-64b'])+['-I',str(ROOT/'module'),'-o',str(image),str(b)]
 r=subprocess.run(command,capture_output=True,text=True);assert r.returncode==0,r.stderr
 return image.read_bytes()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emu',type=Path,required=True);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--runner',default='');a=p.parse_args();total=0
 with tempfile.TemporaryDirectory(prefix='emuinit-boot-',dir=ROOT)as td:
  w=Path(td);root=w/'root';root.mkdir()
  for name in ('env','dev','fd','net','net.alt','prog','srv','chan','mnt','dis/lib','dis/wm'): (root/name).mkdir(parents=True,exist_ok=True)
  awm=root/'dis/wm/awm.dis';shell=root/'dis/sh.dis'
  seeds=[([],['-l']),(['two words',"it's quoted",'λ'],['two words',"it's quoted",'λ']),(['-g640x480','pos'],['pos']),(['-g','800x600','-C','r8g8b8','pos'],['pos']),(['-vg640x480','tail'],['tail']),(['--','-Xdis64-emuinit'],['-Xdis64-emuinit'])]
  for jit in(0,1):
   for selector in('separate','attached','environment'):
    for seed,(extra,wanted)in enumerate(seeds):
     for mode in('missing','primary','fallback','primary-fault','fallback-fault','classic-primary','classic-fallback','hash-primary','hash-fallback'):
       for path in(awm,shell):
        if path.exists():path.unlink()
       selection=['-X','dis64-emuinit']if selector=='separate'else['-Xdis64-emuinit']if selector=='attached'else[]
       args=[str(a.emu.resolve()),'-v','-r',str(root)]+selection+['-c',str(jit)]+extra
       counts=list(map(len,wanted))
       if mode!='missing':
        fallback='fallback'in mode;target=shell if fallback else awm
        image=bytearray(provider(a.limbo.resolve(),w,([2,2,2]+counts)if fallback else counts,fault='fault'in mode,classic='classic'in mode))
        if 'hash'in mode:
         tag=bytes.fromhex('face0186');assert image.count(tag)==1;image[image.index(tag)+7]^=1
        target.write_bytes(image)
       saved=os.environ.get('EMU');os.environ['EMU']='-X dis64-emuinit'if selector=='environment'else''
       try:output=BOOT['execute'](shlex.split(a.runner)+args)
       except RuntimeError as error:raise RuntimeError(str((jit,selector,seed,mode))+': '+str(error))from error
       finally:
        if saved is None:os.environ.pop('EMU',None)
        else:os.environ['EMU']=saved
       context=(jit,selector,seed,mode);(w/('-'.join(map(str,context))+'.log')).write_text(output)
       expected='fail:error'if mode=='missing'or'classic'in mode or'hash'in mode else'zero divide'if'fault'in mode else''
       marker='native64 scalar module '+('Command'if'fault'in mode else'Emuinit')+': jit='+str(jit)+', exit="'+expected+'"'
       assert marker in output and BOOT['RETIRED']in output,(context,output)
       assert 'emuinit: cannot load /dis/lib/arg.dis'not in output,(context,output)
       assert ('emuinit: unable to load'in output)==(expected=='fail:error'),(context,output)
       assert 'panic:'not in output and 'segmentation violation'not in output,(context,output)
       total+=1
    print('PASS real Arg/emuinit boot: jit='+str(jit),selector,'six argv cases and nine file/fault paths',flush=True)
  # Profile conflicts must be refused before startup; a legacy -d cannot replace
  # the compile-time checked entry even if it happens to name an existing file.
  command=shlex.split(a.runner)+[str(a.emu.resolve()),'-X','dis64-emuinit','-d','/dis/emuinit.dis']
  r=subprocess.run(command,capture_output=True,text=True,timeout=20,start_new_session=True,env=dict(os.environ,EMU=""))
  assert 'select one experimental profile'in r.stdout+r.stderr and 'Initial Dis:'not in r.stdout+r.stderr,(r.returncode,r.stdout,r.stderr)
  duplicate=shlex.split(a.runner)+[str(a.emu.resolve()),'-X','dis64-emuinit','-Xdis64-emuinit']
  r=subprocess.run(duplicate,capture_output=True,text=True,timeout=20,start_new_session=True,env=dict(os.environ,EMU=""))
  assert r.returncode and 'Usage:'in r.stdout+r.stderr and 'Initial Dis:'not in r.stdout+r.stderr
 print('PASS real checked emuinit boot:',total,'C/JIT processes; real unchanged Arg and Sys bind/open/fstat/read, typed formatter, signed-policy file reader, AWM-first checked Command.init, shell fallback, wrong-body, canonical-hash and classic-file controls; no AWM window',flush=True)
if __name__=='__main__':main()
