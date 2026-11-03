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
    parser.add_argument('--native64-sys-io-boot',action='store_true',help='register eight trusted fixed Sys I/O/UTF exports; no file imports')
    parser.add_argument('--native64-freetype-boot',action='store_true',help='register six trusted Freetype exports during Linux scalar startup; no file imports')
    parser.add_argument('--native64-math-boot',action='store_true',help='register trusted Math during experimental scalar startup; no file imports')
    parser.add_argument('--native64-scalar-boot',action='store_true',help='experimental Linux console or Android ARM64 framebuffer probe for checked scalar Dis64 images only')
    parser.add_argument('--native64-caller-guard',action='store_true',help='coherent synchronous caller lease experiment; requires scalar and Math startup, no file calls')
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
    if (args.native64_math_boot or args.native64_sys_io_boot or args.native64_freetype_boot) and not args.native64_scalar_boot:
        parser.error('Trusted builtin startup requires --native64-scalar-boot')
    scalar_linux = target=='Linux' and arch in ('amd64','arm64','riscv64') and args.console
    scalar_android = target=='Android' and arch=='arm64' and args.renderer=='framebuffer' and not args.console
    if args.native64_scalar_boot and not (scalar_linux or scalar_android):
        parser.error('Scalar Dis64 startup requires a 64-bit Linux console or Android ARM64 framebuffer probe')
    if args.native64_scalar_boot and scalar_android and (args.native64_math_boot or args.native64_sys_io_boot or args.native64_freetype_boot or args.native64_caller_guard):
        parser.error('The Android ARM64 probe is builtin-free; trusted native modules are not admitted')
    if args.native64_caller_guard and not (args.native64_scalar_boot and args.native64_math_boot):
        parser.error('Caller guard requires --native64-scalar-boot and --native64-math-boot')
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
    # Optional headless font linkage and generated dependencies must not survive deselection.
    text=assign(text,'NATIVE64_FREETYPE_DIR','libfreetype' if args.native64_freetype_boot else '')
    text=assign(text,'NATIVE64_FREETYPE_LIBS','freetype' if args.native64_freetype_boot else '')
    text=assign(text,'NATIVE64_BOOT_FREETYPE_HEADERS',args.root+'/libinterp/freetype-native64.h' if args.native64_freetype_boot else '')
    if not args.native64_scalar_boot and re.search(r'^DISMODELFLAGS=.*DIS_NATIVE64_SCALAR_BOOT',text,re.M):
        text=assign(text,'DISMODELFLAGS','')
    # Compose the model once so later selections cannot discard lifetime guards.
    if args.native64_scalar_boot:
        flags=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DDIS_NATIVE64_SCALAR_BOOT=1']
        if args.native64_sys_io_boot:
            flags.append('-DDIS_NATIVE64_BOOT_SYS_IO=1')
            text=assign(text,'NATIVE64_BOOT_SYS_HEADERS',args.root+'/libinterp/sys-native64.h')
        if args.native64_math_boot:
            flags.append('-DDIS_NATIVE64_BOOT_MATH=1')
            text=assign(text,'NATIVE64_BOOT_HEADERS','math-native64.h')
        if args.native64_freetype_boot:
            flags.append('-DDIS_NATIVE64_BOOT_FREETYPE=1')
        if args.native64_caller_guard:
            flags.extend(['-DDIS_NATIVE64_FIXED_MATH_CALL=1','-DDIS_NATIVE64_CALLER_GUARD=1'])
        text=assign(text,'DISMODELFLAGS',"'"+' '.join(flags)+"'")
    if args.console:
        text+='\n'+marker+'<$ROOT/mkfiles/profile-console\n# end configure.py flavour\n'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(mode='w',dir=args.output.parent,delete=False) as stream:
        stream.write(text)
        temporary=Path(stream.name)
    os.replace(temporary,args.output)
    print('Prepared',args.output,'for',args.host,'->',target,arch, 'console' if args.console else 'platform default')
    if args.native64_scalar_boot:
        print('Experimental scalar Dis64 startup selected; builtin imports and legacy images are disabled. Rebuild all runtime objects.')
        if scalar_android:
            print('Android probe archive: mk CONF=emu-scalar $ROOT/Android/arm64/lib/emu.a; this does not start AWM.')
        if args.native64_math_boot:
            print('Trusted fixed-frame Math registration enabled; generated metadata is built by host Limbo through mk.')
        if args.native64_sys_io_boot:
            print('Trusted Sys I/O profile enabled: eight fixed exports; shared metadata is built by host Limbo through mk.')
        if args.native64_freetype_boot:
            print('Trusted Freetype enabled: six fixed exports, headless font library and selected generated declarations; file imports stay disabled.')
        if args.native64_caller_guard:
            print('Synchronous caller lease selected in all owning units; file CALL/MCALL admission remains disabled.')
    elif arch in ('amd64','arm64','riscv64'):
        print('64-bit toolchain profile selected; native builtin ABI registration and full VM boot remain pending.')
    print('Build with the classic commands: mk mkdirs; mk emuinstall; mk install')


if __name__=='__main__':
    main()
