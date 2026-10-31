#!/usr/bin/env python3
"""Prepare mkconfig without replacing the classic mk build."""
import argparse
from pathlib import Path
import platform
import re
import tempfile
import os

ROOT = Path(__file__).resolve().parents[1]
SYSTEMS = {'linux':'Linux', 'windows':'Nt', 'nt':'Nt', 'mac':'MacOSX', 'macos':'MacOSX', 'darwin':'MacOSX', 'android':'Android'}
ARCHES = {'x86_64':'amd64', 'x64':'amd64', 'aarch64':'arm64', 'x86':'386', 'i386':'386', 'i686':'386', 'armv7l':'arm'}


# Normalize common names into the platform names already consumed by mk.
def system(name):
    result=SYSTEMS.get(name.lower())
    if result is None:
        raise argparse.ArgumentTypeError('Unsupported host/target: '+name)
    return result


# Keep literal values representable in classic mk assignments and include paths.
def literal(value):
    if not value or re.search(r"[\s#$'\"`{}<>]",value):
        raise argparse.ArgumentTypeError('Classic mk requires a literal path/value without whitespace or mk metacharacters: '+value)
    return value.replace('\\','/')


# Change only selected assignments, retaining the user's comments and other settings.
def assign(text,key,value):
    replacement=key+'='+value
    if re.search('^'+key+'=',text,re.M):
        return re.sub('^'+key+'=.*$',lambda match:replacement,text,flags=re.M)
    return replacement+'\n'+text


# Validate an existing profile and atomically prepare the same mkconfig used by manual builds.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',type=system,default=system(platform.system()))
    parser.add_argument('--target',type=system)
    parser.add_argument('--arch',default=ARCHES.get(platform.machine().lower(),platform.machine().lower()))
    parser.add_argument('--root',type=literal,default=literal(str(ROOT)))
    parser.add_argument('--cross',type=lambda value:literal(value) if value else '')
    parser.add_argument('--ndk',type=literal)
    parser.add_argument('--console',action='store_true')
    parser.add_argument('--renderer',choices=('framebuffer','gles'),default='framebuffer')
    parser.add_argument('--output',type=Path,default=ROOT/'mkconfig')
    args=parser.parse_args()
    target=args.target or args.host
    arch=ARCHES.get(args.arch.lower(),args.arch.lower())
    if not re.fullmatch('[a-z0-9]+',arch):
        parser.error('Invalid architecture name')
    if not (ROOT/('mkfiles/mkhost-'+args.host)).is_file():
        parser.error('No host mk profile for '+args.host)
    if target=='Android' and args.host!='Linux':
        parser.error('These Android toolchain profiles currently use a Linux build host')
    profile=ROOT/('mkfiles/mkfile-'+target+'-'+arch)
    if not profile.is_file():
        parser.error('No target profile: '+str(profile.relative_to(ROOT))+'. A new CPU/platform port must be implemented first.')
    if args.renderer=='gles' and target!='Android':
        parser.error('The GLES renderer currently selects the Android external-window ABI only')
    if args.console and target not in ('Linux','MacOSX','Nt'):
        parser.error('Terminal emulator entries currently exist for Linux, Windows and macOS only')
    if args.host!=target and target!='Android':
        parser.error('Hosted emulators require matching host and target; cross builds need a dedicated toolchain/port')
    text=(ROOT/'mkconfig').read_text()
    marker='# configure.py selected build flavour\n'
    if marker in text:
        text=text[:text.index(marker)]+text[text.index('# end configure.py flavour\n')+len('# end configure.py flavour\n'):]
    for key,value in [('ROOT',args.root),('SYSHOST',args.host),('SYSTARG',target),('OBJTYPE',arch),('EMUMKFILE','mkfile')]:
        text=assign(text,key,value)
    if target=='Android':
        text=assign(text,'WINDOW_BACKEND','andr')
        text=assign(text,'EXTWINFLAGS','-DEXT_WIN' if args.renderer=='gles' else '')
    if args.cross is not None:
        text=assign(text,'CROSS',args.cross)
    if args.ndk is not None:
        text=assign(text,'ANDROID_NDK_HOME',args.ndk)
    if args.console:
        text+='\n'+marker+'<$ROOT/mkfiles/profile-console\n# end configure.py flavour\n'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(mode='w',dir=args.output.parent,delete=False) as stream:
        stream.write(text)
        temporary=Path(stream.name)
    os.replace(temporary,args.output)
    print('Prepared',args.output,'for',args.host,'->',target,arch, 'console' if args.console else 'platform default')
    if arch in ('amd64','arm64','riscv64'):
        print('64-bit toolchain profile selected; complete emu assembly/FPU/signal platform ports remain separate work.')
    print('Build with the classic commands: mk mkdirs; mk emuinstall; mk install')


if __name__=='__main__':
    main()
