#!/usr/bin/env python3
"""Build the Android ARM64 GUI and optionally package the AWM desktop and panel."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from zipfile import ZipFile, ZIP_STORED

ROOT = Path(__file__).resolve().parents[2]


def run(command, cwd, env, log, timeout=None):
    """Keep complete build evidence and expose the failing command's diagnostic tail."""
    print('RUN', log.name, flush=True)
    with log.open('w') as stream:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode:
        print('\n'.join(log.read_text(errors='replace').splitlines()[-35:]), file=sys.stderr)
        raise RuntimeError('Failed '+str(log))
    print('PASS', log.name, flush=True)


def stage_awm_runtime(core, app, build, mk, env, logs):
    for directory in ('lib', 'wm', 'sh'):
        (core/'dis'/directory).mkdir(parents=True, exist_ok=True)
    for directory in ('alib', 'cmd', 'cmd/sh', 'wm'):
        run([str(mk), 'install', 'DIRS='], core/'appl'/directory, env,
            logs/('dis-'+directory.replace('/', '-')+'.log'))
    assets = build/'assets'
    run([sys.executable, str(app/'tools/stage-inferno.py'), '--source', str(core),
         '--output', str(assets/'inferno')], app, env, logs/'stage-runtime.log')
    for module in ('awmpanel-keyboard', 'awmcalc', 'awmfiles', 'awmkeys', 'limboedit', 'edit'):
        if not (assets/f'inferno/dis/wm/{module}.dis').is_file():
            raise RuntimeError(f'Missing staged AWM module {module}')
    if not (assets/'inferno/dis/lib/dialog.dis').is_file():
        raise RuntimeError('Missing staged Dialog module')
    return assets


def test_awm_framebuffer(llvm, runner, archives, assets, app, build, env, logs):
    # Select platform libm first in the static test link. The APK uses shared libm.
    libraries = ('emu', 'libinterp', 'liblodepng', 'libbio', 'libdraw', 'libdynld',
                 'libkeyring', 'libmath', 'libmemlayer', 'libmemdraw', 'libsec',
                 'libtk', 'libfreetype', 'libmp', 'lib9')
    executable = build/'awm-framebuffer'
    launcher = app/'app/src/main/cpp'
    run([str(llvm/'aarch64-linux-android29-clang'), '-O2', '-g', '-static',
         '-DINFERNO_BUNDLE_RUNTIME=1', '-DINFERNO_AWM_RUNTIME=1', '-I'+str(launcher),
         str(ROOT/'utils/tests/android-awm-framebuffer.c'), str(launcher/'inferno_storage.c'),
         str(launcher/'inferno_touch.c'),
         *(f'-Wl,--wrap={symbol}' for symbol in
           ('inferno_android_root', 'stat', 'access', 'mkdir', 'fopen', 'rename', 'unlink')),
         '-Wl,--start-group', '-lm',
         *(str(archives/(name+'.a')) for name in libraries), '-Wl,--end-group',
         '-o', str(executable)], build, env, logs/'framebuffer-link.log')
    bundle = build/'framebuffer-assets'
    shutil.copytree(assets/'inferno', bundle/'inferno')
    home = bundle/'inferno/usr/inferno'
    shutil.rmtree(home)
    documents = home/'Documents'
    documents.mkdir(parents=True)
    (documents/'alpha.txt').write_text('Inferno file preview alpha\n', encoding='utf-8')
    (documents/'notes;[later].txt').write_text(
        'Safe text with ; braces { } and [brackets].\n' + 'Long preview line.\n'*300,
        encoding='utf-8')
    for index in range(12):
        (documents/f'z{index:02d}.txt').write_text(
            f'Wheel fixture {index}\n' + f'Content from fixture {index}.\n'*20, encoding='utf-8')
    picker_fixture = documents/'zsample.b'
    picker_fixture.write_text(
        'implement Sample;\nSample: module { init: fn(); };\n'
        'init() {\n\t# marker\n\tvalue := 16rAA;\n\ttext := "# literal";\n'
        '\tcomple;\n}\n'
        + '# Padding for the function jump.\n'*35
        + 'completion_target() { }\n', encoding='utf-8')
    limbo = home/'Limbo'
    limbo.mkdir()
    edit_fixture = limbo/'example ;[one].b'
    edit_fixture.write_text(
        'implement Example;\nExample: module { init: fn(); };\n'
        'init() {\n\t# open from Files\n\tvalue := 16rAA;\n\ttext := "# literal";\n'
        '\tcomple;\n}\ncompletion_target() { }\n', encoding='utf-8')
    manifest = bundle/'inferno/manifest.txt'
    entries = [line for line in manifest.read_text().splitlines()
               if not line.split('\t', 1)[1].startswith('usr/inferno/')]
    for directory in (documents, limbo):
        entries.append(f'D\t{directory.relative_to(bundle/"inferno").as_posix()}')
        entries.extend(f'F\t{file.relative_to(bundle/"inferno").as_posix()}'
                       for file in sorted(directory.iterdir()))
    manifest.write_text('\n'.join(entries)+'\n')
    files = build/'sdcard'
    files.mkdir()
    run([runner, str(executable), str(bundle), str(files)], logs, env,
        logs/'framebuffer-run.log', timeout=60)
    for fixture in (picker_fixture, edit_fixture):
        edited = files/'Inferno'/fixture.relative_to(bundle/'inferno')
        expected = fixture.read_bytes().replace(b'\tcomple;\n', b'\tcompletion_target;\n')
        if edited.read_bytes() != expected:
            raise RuntimeError(f'Edit modified source bytes outside the completed identifier in {fixture.name}')



def package_awm(apk_tools, platform, signing_tools, app, build, assets, library, revisions, env, logs):
    android = '{http://schemas.android.com/apk/res/android}'
    ET.register_namespace('android', android[1:-1])
    resources = app/'app/src/main'
    manifest = ET.parse(resources/'AndroidManifest.xml')
    root = manifest.getroot()
    core, launcher = revisions
    root.set('package', f'bhgv.vitanuova.inferno_os.awm64.c{core[:12]}.a{launcher[:12]}')
    root.set(android+'versionName', core[:12])
    storage_permissions = {'READ_EXTERNAL_STORAGE', 'WRITE_EXTERNAL_STORAGE', 'MANAGE_EXTERNAL_STORAGE'}
    application = root.find('application')
    application.set(android+'extractNativeLibs', 'false')
    application.set(android+'label', 'Inferno AWM ARM64 test')
    activity = application.find('activity')
    activity.set(android+'label', 'Inferno AWM ARM64 test')
    activity.set(android+'exported', 'true')
    manifest_path = build/'AndroidManifest.xml'
    manifest.write(manifest_path, encoding='utf-8', xml_declaration=True)

    unsigned = build/'unsigned.apk'
    jarsigned = build/'v1.apk'
    aligned = build/'aligned.apk'
    signed = build/'signed.apk'
    key = build/'test-key.pem'
    cert = build/'test-cert.pem'
    apk = build/'Inferno-AWM-arm64-v8a-test.apk'
    min_sdk = '21'
    run([str(apk_tools/'aapt'), 'package', '-f', '-M', str(manifest_path), '-S',
         str(resources/'res'), '-A', str(assets), '-I', str(platform),
         '--min-sdk-version', min_sdk, '--target-sdk-version', '28', '-F', str(unsigned)],
        build, env, logs/'apk-package.log')
    native = 'lib/arm64-v8a/libinferno_os_app.so'
    with ZipFile(unsigned, 'a') as archive:
        archive.write(library, native, compress_type=ZIP_STORED)
    run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '3650',
         '-subj', '/CN=Inferno AWM ARM64 test', '-keyout', str(key), '-out', str(cert)],
        build, env, logs/'apk-key.log')
    signer = [str(signing_tools/'apksign'), '-key', str(key), '-cert', str(cert)]
    # Include the v1 entries in alignment before applying the whole-APK signatures.
    run([*signer, '-v1', '-v3=false', '-in', str(unsigned), '-out', str(jarsigned)],
        build, env, logs/'apk-sign-v1.log')
    run([str(apk_tools/'zipalign'), '-P', '16', '-f', '4', str(jarsigned), str(aligned)],
        build, env, logs/'apk-align.log')
    run([*signer, '-in', str(aligned), '-out', str(signed)], build, env, logs/'apk-sign.log')
    verification = logs/'apk-verify.log'
    run([str(signing_tools/'apksigverify'), '-min-sdk', min_sdk, str(signed)],
        build, env, verification)
    if not all(status in verification.read_text() for status in
               ('v1:   verified=true', 'v2:   present=true verified=true', 'v3:   present=true verified=true')):
        raise RuntimeError('AWM APK is missing a verified v1, v2 or v3 signature')
    run([str(apk_tools/'zipalign'), '-c', '-P', '16', '4', str(signed)], build, env,
        logs/'apk-verify-align.log')
    run([sys.executable, str(app/'tools/verify-apk-abi.py'), str(signed), '--abi', 'arm64-v8a'],
        app, env, logs/'apk-abi.log')
    permissions = subprocess.check_output(
        [str(apk_tools/'aapt'), 'dump', 'permissions', str(signed)], text=True)
    if any('android.permission.'+name not in permissions for name in storage_permissions):
        raise RuntimeError('AWM APK is missing shared-storage permissions')
    with ZipFile(signed) as archive:
        if archive.testzip() is not None or any(name.endswith('.dex') for name in archive.namelist()):
            raise RuntimeError('Invalid native-only AWM APK')
        if archive.getinfo(native).compress_type != ZIP_STORED or archive.read(native) != library.read_bytes():
            raise RuntimeError('Packaged native library differs from the verified AArch64 link')
        for asset in (assets/'inferno').rglob('*'):
            if asset.is_file() and archive.getinfo('assets/'+asset.relative_to(assets).as_posix()).file_size != asset.stat().st_size:
                raise RuntimeError('Packaged runtime asset differs in size: '+str(asset))
    checksum = hashlib.sha256(signed.read_bytes()).hexdigest()
    signed.rename(apk)
    apk.with_suffix('.apk.sha256').write_text(f'{checksum}  {apk.name}\n')
    return dict(apk=apk.name, apk_sha256=checksum, apk_signature_verified=True)


def main():
    """Use an empty output tree, one WORD32/ref64 model and the real NativeActivity frontend."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--host-tools', type=Path, required=True)
    parser.add_argument('--app-root', type=Path, required=True, help='Inferno-OS_Android source checkout')
    parser.add_argument('--build-root', type=Path, required=True, help='empty directory outside both checkouts')
    parser.add_argument('--native64-formatter', action='store_true', help='link the four typed Sys C gateways; no variadic bytecode admission')
    parser.add_argument('--native64-sys-fixed', action='store_true', help='register all 39 fixed Sys exports; variadic and AWM startup remain unavailable')
    parser.add_argument('--native64-draw', action='store_true', help='select native64 Draw C frames/registration; full GUI startup stays guarded')
    parser.add_argument('--native64-tk', action='store_true', help='select native64 Tk C frames/registration; full GUI startup stays guarded')
    parser.add_argument('--native64-awm', action='store_true', help='select the compiled-in native64 AWM desktop and panel')
    parser.add_argument('--apk-sdk', type=Path, help='package the AWM test using Android platform and build-tools 35')
    parser.add_argument('--apk-sign-tools', type=Path, help='directory with native apksign and apksigverify')
    parser.add_argument('--awm-runner', help='run the real Bionic framebuffer smoke test with this qemu-aarch64 executable')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--ninja', default='ninja')
    args = parser.parse_args()
    if args.native64_formatter and not args.native64_sys_fixed:
        parser.error("Typed Sys formatting requires the generated fixed Sys profile")
    if args.native64_sys_fixed and not (args.native64_tk and args.native64_draw):
        parser.error("GUI fixed Sys selection requires coherent Draw and Tk")
    if args.native64_tk != args.native64_draw:
        parser.error("Draw and Tk must be selected together: devgui directly calls both interfaces")
    if args.native64_awm and not args.native64_formatter:
        parser.error("Native64 AWM startup requires --native64-formatter")
    if args.apk_sdk and not args.native64_awm:
        parser.error("--apk-sdk requires --native64-awm")
    if args.awm_runner:
        runner = shutil.which(args.awm_runner)
        if not runner or not args.native64_awm:
            parser.error("--awm-runner requires an executable runner and --native64-awm")
    if args.apk_sdk:
        apk_tools = args.apk_sdk.resolve()/'build-tools/35.0.0'
        platform = args.apk_sdk.resolve()/'platforms/android-35/android.jar'
        if not platform.is_file() or not all(os.access(apk_tools/name, os.X_OK)
                                             for name in ('aapt', 'zipalign')):
            parser.error('Install Android platform 35 and build-tools 35.0.0 in --apk-sdk')
        if not args.apk_sign_tools or not all(os.access(args.apk_sign_tools/name, os.X_OK)
                                             for name in ('apksign', 'apksigverify')):
            parser.error('--apk-sdk requires native apksign and apksigverify in --apk-sign-tools')
        signing_tools = args.apk_sign_tools.resolve()
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
        flags += ' -DDIS_NATIVE64_GUI_DRAW=1' if args.native64_draw else ''
        flags += ' -DDIS_NATIVE64_GUI_SYS_FIXED=1' if args.native64_sys_fixed else ''
        flags += ' -DDIS_NATIVE64_FORMATTER=1' if args.native64_formatter else ''
        if args.native64_awm:
            flags += ' -DDIS64_PILOT_LOADER=1 -DDIS_NATIVE64_AWM_BOOT=1'
            stream.write('NATIVE64_BOOT_AWM_HEADERS='+str(core/'libinterp/awm-native64.h')+'\n')
        stream.write("\nDISMODELFLAGS='"+flags+"'\n")
        if args.native64_tk:
            stream.write('NATIVE64_BOOT_TK_HEADERS='+str(core/'libinterp/tk-native64.h')+'\n')
        if args.native64_draw:
            stream.write('NATIVE64_BOOT_DRAW_HEADERS='+str(core/'libinterp/draw-native64.h')+'\n')
            stream.write('NATIVE64_BOOT_GUI_HEADERS='+str(core/'libinterp/gui-native64.h')+'\n')
        if args.native64_sys_fixed:
            stream.write('NATIVE64_BOOT_SYS_HEADERS='+str(core/'libinterp/sys-native64.h')+'\n')
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
         '-DINFERNO_BUNDLE_RUNTIME='+('ON' if args.native64_awm else 'OFF'),
         '-DINFERNO_AWM_RUNTIME='+('ON' if args.native64_awm else 'OFF'),
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
    required = ['ANativeActivity_onCreate', 'android_main', 'amain', 'ioa_pointer_motion', 'xkbd_key']
    if args.native64_awm:
        required += ['native64awmboot', 'inferno_android_root', 'ioa_prepare_storage_progress']
    if args.native64_formatter:
        required += ['Sys_'+name+'64' for name in ('print', 'fprint', 'sprint', 'aprint')]
        required += ['Sys_bindframe64', 'Sys_setframes64', 'Sys_clearframes64', 'Sys_bindsite64', 'Sys_callsite64']
        required += ['Sys_'+name+'frame64' for name in ('print', 'fprint', 'sprint', 'aprint')]
    for name in required:
        if name not in {line.split()[-1] for line in symbols.splitlines() if line.split()}:
            raise RuntimeError('Missing native GUI entry: '+name)
    digest = hashlib.sha256()
    for raw in sorted(name for name in names if name):
        digest.update(raw+b'\0')
        digest.update(hashlib.sha256((ROOT/os.fsdecode(raw)).read_bytes()).digest())
    dirty = subprocess.run(['git', 'diff', '--quiet', 'HEAD'], cwd=ROOT).returncode
    if dirty not in (0, 1):
        raise RuntimeError('Cannot inspect core source provenance')
    report = dict(typed_sys_formatter_linked=args.native64_formatter, sys_fixed_native64_selected=args.native64_sys_fixed, draw_native64_selected=args.native64_draw, tk_native64_selected=args.native64_tk, core_worktree_dirty=bool(dirty), core_tracked_inputs_sha256=digest.hexdigest(), ndk=(ndk/'source.properties').read_text(),
                  core_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  app_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=app, text=True).strip(),
                  library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),
                  gui_native_linked=True, awm_boot_selected=args.native64_awm,
                  awm_panel_selected=args.native64_awm, awm_started=False)
    result = build/'result.json'
    result.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS complete Android ARM64 GUI native link', flush=True)
    if args.apk_sdk or args.awm_runner:
        assets = stage_awm_runtime(core, app, build, tools/'mk', env, logs)
    if args.awm_runner:
        test_awm_framebuffer(llvm, runner, archives, assets, app, build, env, logs)
        report.update(awm_started=True, awm_panel_interaction_verified=True,
                      awm_files_interaction_verified=True, awm_shared_storage_verified=True,
                      awm_android_touch_verified=True, awm_android_mouse_verified=True,
                      awm_android_keyboard_verified=True, awm_screen_keyboard_verified=True,
                      awm_editor_verified=True, awm_files_editor_verified=True,
                      awm_editor_picker_verified=True)
        result.write_text(json.dumps(report, indent=2)+'\n')
    if args.apk_sdk:
        report.update(package_awm(apk_tools, platform, signing_tools, app, build, assets, library,
                                  (report['core_revision'], report['app_revision']), env, logs))
        result.write_text(json.dumps(report, indent=2)+'\n')
        print('PASS signed Android ARM64 AWM test APK', flush=True)


if __name__ == '__main__':
    main()
