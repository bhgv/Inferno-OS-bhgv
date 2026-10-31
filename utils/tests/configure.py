#!/usr/bin/env python3
"""Exercise configuration selection and actual console device-list generation."""
from pathlib import Path
import subprocess
import tempfile
import os

ROOT=Path(__file__).resolve().parents[2]


# Check generated mk inputs without claiming unavailable Windows/macOS toolchains were executed.
def main():
    original=(ROOT/'mkconfig').read_bytes()
    with tempfile.TemporaryDirectory(prefix='configure-',dir=ROOT) as directory:
        work=Path(directory)
        for host,arch in [('linux','386'),('windows','386'),('macos','386')]:
            output=work/(host+'.mkconfig')
            subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host',host,'--arch',arch,'--console','--output',str(output)],check=True,capture_output=True)
            text=output.read_text()
            if '<$ROOT/mkfiles/profile-console' not in text or 'OBJTYPE=386' not in text:
                raise RuntimeError('Wrong console configuration')
        for renderer,flag in [('framebuffer',''),('gles','-DEXT_WIN')]:
            output=work/(renderer+'.mkconfig')
            subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--target','android','--arch','arm','--renderer',renderer,'--output',str(output)],check=True,capture_output=True)
            if 'EXTWINFLAGS='+flag+'\n' not in output.read_text():
                raise RuntimeError('Mixed framebuffer/GLES window ABI')
        bad=subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','windows','--arch','amd64','--output',str(work/'bad')],capture_output=True)
        if bad.returncode==0 or (work/'bad').exists():
            raise RuntimeError('Missing platform port was advertised as available')
        for target in ('Linux','MacOSX','Nt'):
            result=subprocess.run(['/bin/sh','../port/mkdevlist','emu-g'],cwd=ROOT/'emu'/target,env=dict(os.environ,ROOT=str(ROOT),AWK='awk'),capture_output=True,text=True,check=True)
            devs=next(row for row in result.stdout.splitlines() if row.startswith('DEVS='))
            libs=next(row for row in result.stdout.splitlines() if row.startswith('LIBS='))
            if any(name in devs for name in ('devdraw','devpointer','devgui')) or any(name in libs.split() for name in ('tk','freetype','draw','memdraw','memlayer')):
                raise RuntimeError('Console profile includes GUI components: '+target)
    if (ROOT/'mkconfig').read_bytes()!=original:
        raise RuntimeError('Configuration tests changed the user mkconfig')
    print('PASS classic configuration: three console platforms, actual device lists, distinct Android renderer ABIs, missing-port rejection')


if __name__=='__main__':
    main()
