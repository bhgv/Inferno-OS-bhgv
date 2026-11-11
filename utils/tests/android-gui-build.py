#!/usr/bin/env python3
"""Build the complete Android ARM64 GUI native graph; this does not grant AWM admission."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def run(command, cwd, env, log):
    """Keep complete build evidence and expose the failing command's diagnostic tail."""
    print('RUN', log.name, flush=True)
    with log.open('w') as stream:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print('\n'.join(log.read_text(errors='replace').splitlines()[-35:]), file=sys.stderr)
        raise RuntimeError('Failed '+str(log))
    print('PASS', log.name, flush=True)


def main():
    """Use an empty output tree, one WORD32/ref64 model and the real NativeActivity frontend."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--host-tools', type=Path, required=True)
    parser.add_argument('--app-root', type=Path, required=True, help='Inferno-OS_Android source checkout')
    parser.add_argument('--build-root', type=Path, required=True, help='empty directory outside both checkouts')
    parser.add_argument('--native64-tk', action='store_true', help='select native64 Tk C frames/registration; full GUI startup stays guarded')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--ninja', default='ninja')
    args = parser.parse_args()
    build, tools, ndk, app = (path.resolve() for path in
                             (args.build_root, args.host_tools, args.ndk, args.app_root))
    for source in (ROOT, app):
        if build == source or source in build.parents or build in source.parents:
            parser.error('Build directory must be separate from both source checkouts')
    if build.exists() and any(build.iterdir()):
        parser.error('Build directory must be empty; incompatible objects must not be reused')
    for name in ('mk', 'limbo', 'iyacc', 'decgen'):
        if not os.access(tools/name, os.X_OK):
            parser.error('Missing host tool: '+str(tools/name))
    llvm = ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin'
    if not (llvm/'aarch64-linux-android21-clang').is_file():
        parser.error('Expected a modern Linux NDK with the API21 AArch64 compiler')
    if not (app/'app/src/main/cpp/CMakeLists.txt').is_file():
        parser.error('Missing Android NativeActivity source')
    sys.path.insert(0, str(ROOT/'utils'))
    import configure
    for path in (build, tools, ndk):
        configure.literal(str(path))
    ninja = shutil.which(args.ninja)
    if ninja is None:
        parser.error('Missing Ninja build tool')
    build.mkdir(parents=True, exist_ok=True)
    core = build/'core'
    # Read current tracked sources rather than git archives, preserving reviewed local fixes.
    names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).split(b'\0')
    for raw in names:
        if not raw:
            continue
        relative = Path(os.fsdecode(raw))
        target = core/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT/relative, target, follow_symlinks=False)
    logs = build/'logs'; logs.mkdir()
    temporary = build/'tmp'; temporary.mkdir()
    env = dict(os.environ, PATH=str(tools)+os.pathsep+os.environ.get('PATH', ''), TMPDIR=str(temporary))
    run([sys.executable, str(core/'utils/configure.py'), '--host', 'linux', '--target', 'android',
         '--arch', 'arm64', '--renderer', 'framebuffer', '--ndk', str(ndk), '--root', str(core)],
        core, env, logs/'configure.log')
    # Compile the full GUI graph, not emu-scalar. Legacy registration stays guarded.
    with (core/'mkconfig').open('a') as stream:
        flags = '-DDIS_WORD32=1'+(' -DDIS_NATIVE64_GUI_TK=1' if args.native64_tk else '')
        stream.write("\nDISMODELFLAGS='"+flags+"'\n")
        if args.native64_tk:
            stream.write('NATIVE64_BOOT_TK_HEADERS='+str(core/'libinterp/tk-native64.h')+'\n')
    binary = core/'Android/arm64/bin'; binary.mkdir(parents=True, exist_ok=True)
    archives = core/'Android/arm64/lib'; archives.mkdir(parents=True, exist_ok=True)
    for name in ('mk', 'limbo', 'iyacc', 'decgen'):
        target = binary/name
        if target.exists() or target.is_symlink():
            target.unlink()
        target.symlink_to(tools/name)
    run([str(tools/'mk'), 'CONF=emu', str(archives/'emu.a'), 'KERNDATE=1794355200'],
        core/'emu/Android', env, logs/'gui-core.log')
    for library in ('libbio', 'libdynld'):
        run([str(tools/'mk'), 'install'], core/library, env, logs/(library+'.log'))
    run([args.cmake, '-S', str(app/'app/src/main/cpp'), '-B', str(build/'native'), '-G', 'Ninja',
         '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', '-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),
         '-DCMAKE_MAKE_PROGRAM='+ninja, '-DANDROID_ABI=arm64-v8a',
         '-DANDROID_PLATFORM=android-21', '-DANDROID_STL=c++_static',
         '-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON', '-DINFERNO_ROOT='+str(core),
         '-DCMAKE_BUILD_TYPE=Debug'], app, env, logs/'cmake.log')
    run([args.cmake, '--build', str(build/'native'), '--parallel', '2'], app, env, logs/'native-link.log')
    library = build/'native/libinferno_os_app.so'
    elf = subprocess.check_output([str(llvm/'llvm-readelf'), '-h', '-l', '-d', str(library)], text=True)
    (logs/'elf.txt').write_text(elf)
    if 'ELF64' not in elf or 'AArch64' not in elf:
        raise RuntimeError('The GUI shared library is not ELF64/AArch64')
    loads = [line.split() for line in elf.splitlines() if line.lstrip().startswith('LOAD ')]
    if not loads or any(int(row[-1], 0) < 16384 for row in loads):
        raise RuntimeError('The GUI shared library lacks 16KiB LOAD alignment')
    symbols = subprocess.check_output([str(llvm/'llvm-nm'), '-D', '--defined-only', str(library)], text=True)
    (logs/'symbols.txt').write_text(symbols)
    for name in ('ANativeActivity_onCreate', 'android_main', 'amain'):
        if name not in {line.split()[-1] for line in symbols.splitlines() if line.split()}:
            raise RuntimeError('Missing native GUI entry: '+name)
    digest = hashlib.sha256()
    for raw in sorted(name for name in names if name):
        digest.update(raw+b'\0')
        digest.update(hashlib.sha256((ROOT/os.fsdecode(raw)).read_bytes()).digest())
    dirty = subprocess.run(['git', 'diff', '--quiet', 'HEAD'], cwd=ROOT).returncode
    if dirty not in (0, 1):
        raise RuntimeError('Cannot inspect core source provenance')
    report = dict(tk_native64_selected=args.native64_tk, core_worktree_dirty=bool(dirty), core_tracked_inputs_sha256=digest.hexdigest(), ndk=(ndk/'source.properties').read_text(),
                  core_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  app_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=app, text=True).strip(),
                  library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),
                  gui_native_linked=True, awm_started=False,
                  remaining='Native64 builtin registration and general managed Dis64 compiler/runtime admission')
    (build/'result.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS complete Android ARM64 GUI native link; AWM startup is not yet admitted', flush=True)


if __name__ == '__main__':
    main()
