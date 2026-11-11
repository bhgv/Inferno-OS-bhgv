#!/usr/bin/env python3
"""Exercise configuration selection and actual console device-list generation."""
from pathlib import Path
import subprocess
import tempfile
import os
import shutil

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
        output=work/'scalar.mkconfig'
        subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch','arm64','--console','--native64-scalar-boot','--output',str(output)],check=True,capture_output=True)
        if "DISMODELFLAGS='-DDIS_WORD32=1 -DDIS64_PILOT_LOADER=1 -DDIS_NATIVE64_SCALAR_BOOT=1'" not in output.read_text():
            raise RuntimeError('Scalar startup lost its coherent guarded model')
        for options in (['--arch','386','--console'], ['--arch','arm64'], ['--target','android','--arch','arm64','--renderer','gles']):
            bad=subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--native64-scalar-boot','--output',str(work/'invalid-scalar')]+options,capture_output=True)
            if bad.returncode==0 or (work/'invalid-scalar').exists():
                raise RuntimeError('Scalar startup accepted an unsupported profile')
        subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch','arm64','--console','--native64-scalar-boot','--native64-math-boot','--output',str(output)],check=True,capture_output=True)
        if 'NATIVE64_BOOT_HEADERS=math-native64.h' not in output.read_text() or '-DDIS_NATIVE64_BOOT_MATH=1' not in output.read_text():
            raise RuntimeError('Math startup lost its generated header dependency')
        bad=subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch','arm64','--console','--native64-math-boot','--output',str(work/'invalid-math')],capture_output=True)
        if bad.returncode==0 or (work/'invalid-math').exists():
            raise RuntimeError('Math startup accepted the legacy ABI')
        subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch','arm64','--console','--native64-scalar-boot','--native64-math-boot','--native64-sys-io-boot','--output',str(output)],check=True,capture_output=True)
        text=output.read_text()
        if 'NATIVE64_BOOT_SYS_HEADERS='+str(ROOT)+'/libinterp/sys-native64.h' not in text or '-DDIS_NATIVE64_BOOT_SYS_IO=1' not in text or '-DDIS_NATIVE64_BOOT_MATH=1' not in text:
            raise RuntimeError('Trusted Sys startup lost its coherent absolute header dependency')
        bad=subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch','arm64','--console','--native64-sys-io-boot','--output',str(work/'invalid-sys')],capture_output=True)
        if bad.returncode==0 or (work/'invalid-sys').exists():
            raise RuntimeError('Trusted Sys startup accepted the legacy ABI')
        # Cover every 64-bit backend and optional Sys combination: a later flag must not erase the lease.
        for arch in ('amd64','arm64','riscv64'):
            for sysio in (False,True):
                command=['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch',arch,'--console',
                         '--native64-scalar-boot','--native64-math-boot','--native64-caller-guard','--output',str(output)]
                if sysio:
                    command.append('--native64-sys-io-boot')
                subprocess.run(command,check=True,capture_output=True)
                lines=output.read_text().splitlines()
                model=[line for line in lines if line.startswith('DISMODELFLAGS=')]
                required=['DIS_WORD32','DIS64_PILOT_LOADER','DIS_NATIVE64_SCALAR_BOOT',
                          'DIS_NATIVE64_BOOT_MATH','DIS_NATIVE64_FIXED_MATH_CALL','DIS_NATIVE64_CALLER_GUARD']
                if len(model)!=1 or any(model[0].count('-D'+name+'=1')!=1 for name in required):
                    raise RuntimeError('Caller lease flags are not coherent: '+arch)
                if ('-DDIS_NATIVE64_BOOT_SYS_IO=1' in model[0])!=sysio:
                    raise RuntimeError('Caller lease selected the wrong Sys profile')
                if ('-DDIS_NATIVE64_MANAGED_FILES=1' in model[0])!=sysio:
                    raise RuntimeError('Managed file profile does not follow the selected native console')
                expected='NATIVE64_BOOT_STRING_HEADERS='+ (str(ROOT)+'/libinterp/string-native64.h' if sysio else '')
                if expected not in lines:
                    raise RuntimeError('Managed file profile lost or retained its generated String dependency')
        # The font profile adds a headless archive and a selected native header on all Linux backends.
        for arch in ('amd64','arm64','riscv64'):
            subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch',arch,'--console',
                            '--native64-scalar-boot','--native64-math-boot','--native64-sys-io-boot',
                            '--native64-caller-guard','--native64-freetype-boot','--output',str(output)],check=True,capture_output=True)
            text=output.read_text()
            for item in ('-DDIS_NATIVE64_BOOT_FREETYPE=1','-DDIS_NATIVE64_CALLER_GUARD=1',
                         'NATIVE64_FREETYPE_DIR=libfreetype','NATIVE64_FREETYPE_LIBS=freetype',
                         'NATIVE64_BOOT_FREETYPE_HEADERS='+str(ROOT)+'/libinterp/freetype-native64.h'):
                if item not in text:
                    raise RuntimeError('Font startup lost its coherent dependency: '+item)
        # Reconfiguration from a generated font profile must clear its optional closure and model.
        fake=work/'configuration-source'; fake.mkdir()
        # Exercise the owning script against its current ROOT through an isolated copy of its inputs.
        shutil.copytree(ROOT/'mkfiles',fake/'mkfiles')
        (fake/'utils').mkdir()
        shutil.copy2(ROOT/'utils/configure.py',fake/'utils/configure.py')
        (fake/'mkconfig').write_text(text)
        subprocess.run(['python3',str(fake/'utils/configure.py'),'--host','linux','--arch','386','--console',
                        '--output',str(output)],check=True,capture_output=True)
        cleared=output.read_text()
        if 'DISMODELFLAGS=\n' not in cleared or any(item in cleared for item in
            ('DIS_NATIVE64_BOOT_FREETYPE=1','NATIVE64_FREETYPE_LIBS=freetype','NATIVE64_FREETYPE_DIR=libfreetype')):
            raise RuntimeError('Font deselection preserved a stale native model or archive')
        refused=work/'invalid-font'; refused.write_text('preserve font configuration\n')
        for options in ([], ['--native64-scalar-boot','--arch','386'],
                        ['--native64-scalar-boot','--target','android','--renderer','framebuffer']):
            bad=subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch','arm64','--console',
                                '--native64-freetype-boot','--output',str(refused)]+options,capture_output=True)
            if bad.returncode==0 or refused.read_text()!='preserve font configuration\n':
                raise RuntimeError('Invalid font profile changed an existing configuration')
        # Validation must preserve an existing output as well as refuse creating a new one.
        sentinel=work/'invalid-caller'; sentinel.write_text('preserve user configuration\n')
        for options in ([],['--native64-scalar-boot'],['--native64-math-boot'],
                        ['--native64-scalar-boot','--native64-math-boot','--arch','386'],
                        ['--native64-scalar-boot','--native64-math-boot','--target','android']):
            bad=subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--arch','arm64','--console',
                                '--native64-caller-guard','--output',str(sentinel)]+options,capture_output=True)
            if bad.returncode==0 or sentinel.read_text()!='preserve user configuration\n':
                raise RuntimeError('Invalid caller profile changed an existing configuration')
        android=work/'android64.mkconfig'
        subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--target','android','--arch','arm64',
                        '--native64-scalar-boot','--output',str(android)],check=True,capture_output=True)
        text=android.read_text()
        if text.index('DISMODELFLAGS=')>text.index('<$ROOT/mkfiles/mkfile-') or '-DDIS_NATIVE64_SCALAR_BOOT=1' not in text:
            raise RuntimeError('Android scalar model must precede target profile expansion')
        for extra in (['--renderer','gles'],['--arch','arm'],['--native64-math-boot'],['--native64-sys-io-boot'],['--native64-freetype-boot']):
            bad=subprocess.run(['python3',str(ROOT/'utils/configure.py'),'--host','linux','--target','android','--arch','arm64',
                                '--native64-scalar-boot','--output',str(work/'bad-android64')]+extra,capture_output=True)
            if bad.returncode==0 or (work/'bad-android64').exists():
                raise RuntimeError('Unsupported Android scalar shape was admitted')
        result=subprocess.run(['/bin/sh','../port/mkdevlist','emu-scalar'],cwd=ROOT/'emu/Android',
                              env=dict(os.environ,ROOT=str(ROOT),AWK='awk'),capture_output=True,text=True,check=True)
        if 'scalar-main.' not in result.stdout or 'devdraw' in result.stdout:
            raise RuntimeError('Android scalar configuration must use its dedicated builtin-free entry')
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
