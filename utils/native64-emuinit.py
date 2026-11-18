#!/usr/bin/env python3
"""Generate trusted native startup metadata from the unchanged checked emuinit."""
import argparse,runpy,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def generate(limbo,output):
 if not __debug__:raise RuntimeError('checked source generation requires assertions')
 gate=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'))
 with tempfile.TemporaryDirectory(prefix='emuinit-metadata-')as td:
  w=Path(td);gate['generate'](limbo,w)
  # Existing Command structural signature and canonical hash, from the file producer.
  probe=w/'probe.b';probe.write_text('implement Probe;include "sys.m";include "draw.m";include "sh.m";Probe:module{init:fn();};init(){p:=load Command "command.dis64";p->init(nil,nil);}')
  image=w/'probe.dis64';subprocess.run([str(limbo),'-64c','-I',str(ROOT/'module'),'-o',str(image),str(probe)],check=True)
  sections=gate['CMD']['CMD']['SECTIONS'](image.read_bytes())
  text=(w/'full.h').read_text().replace('fullload','native64emuinitload')
  requests=sections[5];assert int.from_bytes(requests[:4],'big')==1 and requests[16:20]==bytes.fromhex('face0186')
  signature=int.from_bytes(requests[20:24],'big');assert '{"init",'+str(signature)+'U,80,'in text
  for name,values in [('boot_command_requests',sections[5]),('boot_command_names',sections[7])]:text+='static const uchar '+name+'[]={'+','.join(map(str,values))+'};\n'
  output.write_text('/* Generated from unchanged appl/cmd/emuinit.b; trusted native metadata, no file ABI. */\n'+text)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(Path(shutil.which(str(a.limbo)) or a.limbo).resolve(),a.output)
if __name__=='__main__':main()
