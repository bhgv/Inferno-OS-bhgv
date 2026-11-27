#!/usr/bin/env python3
"""Command is an interface, not an implementation name; init is a Limbo call."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CASES = runpy.run_path(str(ROOT/'utils/tests/dis64-command-cases.py'))
CHAIN = runpy.run_path(str(ROOT/'utils/tests/dis64-command-chain-cases.py'))
READ = runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))
BOOT = runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
SECTIONS = runpy.run_path(str(ROOT/'utils/tests/dis64-command.py'))['sections']


def require(ok, detail):
    if not ok:
        raise RuntimeError(detail)


def pool(data):
    words = list(struct.unpack('>'+'I'*(len(data)//4), data)); at = 1; result = []
    for _ in range(words[0]):
        count = words[at]; at += 1
        result.append(words[at:at+count]); at += count
    require(at == len(words), 'pool extent')
    return result


def rebuild(parts):
    at = 32+12*len(parts); directory = bytearray(); payload = bytearray()
    for kind, data in sorted(parts.items()):
        directory += struct.pack('>HHII', kind, 1, at, len(data)); payload += data; at += len(data)
    return b'Dis64\r\n\x1a'+struct.pack('>HHIIIII', 1, 0, 32+12*len(parts), 1, 0, 0, len(parts))+directory+payload


def renamed(data, index, text):
    parts = SECTIONS(data); values = pool(parts[7]); values[index] = list(map(ord, text))
    words = [len(values)]
    for value in values:
        words += [len(value)]+value
    parts[7] = struct.pack('>'+'I'*len(words), *words)
    return rebuild(parts)


def witnesses():
    emu = (ROOT/'appl/cmd/emuinit.b').read_text()
    require(re.search(r'^\s*mod\s*=\s*load Command "/dis/wm/awm\.dis";', emu, re.M), 'emuinit AWM load witness drift')
    require(re.search(r'^\s*mod->init\(nil,\s*args\);', emu, re.M), 'emuinit explicit init witness drift')
    for filename in ('sh.b', 'sh.y'):
        source = (ROOT/'appl/cmd/sh'/filename).read_text()
        require('mod := load Command npath;' in source and 'mod->init(ctxt.drawcontext, argv);' in source and 'mod->init(drawcontext, argv);' in source,
                'shell load/call witness drift: '+filename)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--emu', type=Path)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--runner', default=os.environ.get('NATIVE64_RUNNER', ''))
    args = parser.parse_args()
    if bool(args.emu) != bool(args.root):
        parser.error('--emu and --root are required together')
    witnesses(); env = dict(os.environ); runs = proofs = 0
    temporary = args.root.resolve() if args.root else ROOT
    with tempfile.TemporaryDirectory(prefix='command-alias-', dir=temporary) as td:
        work = Path(td); env['TMPDIR'] = str(work)
        (work/'command.m').write_text(CASES['INTERFACE'])
        (work/'library.m').write_text('Library:module{inc:fn(n:int):int;};')
        provider = work/'library.dis64'; caller = work/'client.dis64'; dependency = work/'dependency.dis64'
        path = '/'+str(provider.relative_to(args.root.resolve())) if args.root else 'library.dis64'
        dep = '/'+str(dependency.relative_to(args.root.resolve())) if args.root else 'dependency.dis64'

        def compile(text, flag, output, compiler=None):
            source = work/'source.b'; source.write_text(text)
            result = subprocess.run([str((compiler or args.limbo).resolve()), flag, '-I', str(ROOT/'module'), '-I', str(work), '-o', str(output), str(source)],
                                    env=env, capture_output=True, text=True)
            require(result.returncode == 0, result.stderr)
            return output.read_bytes()

        cc = shlex.split(env.get('CC', 'cc'))+shlex.split(env.get('NATIVE64_CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        checker = work/'check'
        subprocess.run(cc+['-I'+str(ROOT/'include'), str(ROOT/'utils/tests/dis64-command-proof.c'), '-o', str(checker)], check=True, env=env)
        runner = shlex.split(args.runner)

        def proof(data, mode, expected, client=None):
            nonlocal proofs
            file = work/'proof.bin'; file.write_bytes(data)
            result = subprocess.run(runner+[str(checker), str(file), mode, str(expected)]+([str(client)] if client else []), capture_output=True, text=True, env=env, timeout=90)
            require(not result.returncode and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr, (mode, expected, result.returncode, result.stderr))
            proofs += 1

        # The real Wm export must match callers compiled against module/sh.m.
        assembly = compile((ROOT/'appl/wm/awm.b').read_text(), '-S', work/'actual-awm.s').decode()
        exports = {name: int(value, 16) for value, name in re.findall(r'^\s*link\s+[^,]+,[^,]+,(0x[0-9a-f]+),"([^\"]+)"$', assembly, re.M)}
        require(re.search(r'^\s*module\s+Wm$', assembly, re.M) and 'init' in exports, 'actual Wm export witness drift')
        expected = exports['init']

        selected = {'consume-two', 'callee-cons', 'source-context', 'load-no-init', 'explicit-init-error'}
        rows = [r for r in CASES['cases'](len(path)) if r[0] in selected]
        bad = '{z:=0;bad:=1/z;}'
        providertext = 'implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){if(ctxt!=nil || len argv!=2)'+bad+'first:=hd argv;if(first!="wm")'+bad+'tail:=tl argv;second:=hd tail;if(second!="-probe")'+bad+'}'
        bootstrap = 'implement Emuinit;include "command.m";Emuinit:module{init:fn();};init(){args:="wm"::"-probe"::nil;mod:Command;mod=load Command "library.dis64";if(mod==nil)'+bad+'mod->init(nil,args);if(len args!=2)'+bad+'}'
        rows.insert(0, ('emuinit-call', providertext, bootstrap, 0))
        rows += [next(r for r in CASES['cases'](len(path)) if r[0].startswith('local-'))]
        chain = next(r for r in CHAIN['cases']() if r[0] == 'chain-scalar')
        rows += [chain]
        for implementation in ('Command', 'Wm', 'Application'):
            for row in rows:
                name, text, client, failure = row[:4]
                if name == 'chain-scalar':
                    compile(row[4], '-64b', dependency)
                    text = text.replace('"dependency.dis64"', '"'+dep+'"')
                text = CASES['implementation'](text, implementation)
                client = client.replace('implement Client;', 'implement Emuinit;', 1).replace('Client:module', 'Emuinit:module', 1)
                client = client.replace('"library.dis64"', '"'+path+'"')
                # Positive execution must not silently pass if LOAD returns nil.
                client = client.replace('if(m==nil)return;', 'if(m==nil){z:=0;bad:=1/z;}')
                data = compile(text, '-64b', provider)
                require(data == compile(text, '-64b', work/'again.dis64'), 'provider nondeterminism')
                require(''.join(map(chr, pool(SECTIONS(data)[7])[0])) == implementation, 'implementation label lost')
                require(struct.unpack_from('>I', SECTIONS(data)[6], 24)[0] == expected, 'implementation changed init type hash')
                current = compile(text, '-i', work/'classic.dis')
                if args.baseline:
                    require(current == compile(text, '-i', work/'old.dis', args.baseline), 'classic alias bytes changed')
                    if implementation == 'Command':
                        require(data == compile(text, '-64b', work/'old.dis64', args.baseline), 'existing Command bytes changed')
                compile(client, '-64c', caller)
                imports = READ['legacy_instructions'](compile(client, '-i', work/'client.dis'), with_imports=True)[-1]
                require(any(e['name'] == 'init' and e['signature'] == expected for table in imports for e in table), 'caller/actual Wm signature disagreement')
                proof(data, 'library', 1); proof(data, 'client', 0); proof(data, 'match', 1, caller)
                proof(renamed(data, 0, ''), 'library', 0)
                proof(renamed(data, 0, '\x00'), 'library', 0)
                proof(renamed(data, 0, 'Неверно'), 'library', 0)
                parts = SECTIONS(data); export = bytearray(parts[6]); export[24] ^= 1; parts[6] = bytes(export)
                wronghash = rebuild(parts); proof(wronghash, 'library', 1); proof(wronghash, 'match', 0, caller)
                exportname = struct.unpack_from('>I', SECTIONS(data)[6], 4)[0]
                proof(renamed(data, exportname, 'start'), 'library', 0)
                if name != 'chain-scalar':
                    proof(renamed(data, 2, 'DifferentInterface'), 'library', 0)
                if args.emu:
                    command = runner+[str(args.emu.resolve()), '-v', '-r', str(args.root.resolve())]
                    vmcaller = '/'+str(caller.relative_to(args.root.resolve()))
                    for jit in (0, 1):
                        output = BOOT['execute'](command+['-c', str(jit), '-X', 'dis64-calls', '-d', vmcaller])
                        owner = implementation if failure else 'Emuinit'; error = 'zero divide' if failure else ''
                        require('native64 scalar module '+owner+': jit='+str(jit)+', exit="'+error+'"' in output and BOOT['RETIRED'] in output and 'panic:' not in output and 'faults:' not in output and ('Broken:' in output) == bool(failure), output)
                        runs += 1
                print('PASS Limbo load Command -> '+implementation+'.init:', name, flush=True)
        examples = re.findall(r'```limbo\n(.*?)```', (ROOT/'doc/dis64-command-alias.ru.md').read_text(), re.S)
        require(len(examples) == 3, 'alias tutorial source boundary drift')
        (work/'command.m').write_text(examples[0])
        tutorial = compile(examples[1], '-64b', provider)
        boot = examples[2].replace('"/probe-wm.dis64"', '"'+path+'"')
        compile(boot, '-64c', caller)
        proof(tutorial, 'library', 1); proof(tutorial, 'client', 0); proof(tutorial, 'match', 1, caller)
        if args.emu:
            for jit in (0, 1):
                output = BOOT['execute'](command+['-c', str(jit), '-X', 'dis64-calls', '-d', vmcaller])
                require('native64 scalar module Emuinit: jit='+str(jit)+', exit=""' in output and BOOT['RETIRED'] in output and 'panic:' not in output and 'Broken:' not in output, output)
                runs += 1
        print('PASS compiled Limbo launcher tutorial: Wm implementation / Command interface', flush=True)
        # Producer names retain the same bounded policy; fixed interface identity
        # and full hashes above remain mandatory regardless of implementation.
        sample = next(r for r in CASES['cases'](len(path)) if r[0] == 'consume-two')[1]
        long = CASES['implementation'](sample, 'N'*255)
        proof(compile(long, '-64b', provider), 'library', 1)
        too_long = long.replace('N'*255, 'N'*256)
        compile(too_long, '-i', work/'long.dis')
        source = work/'source.b'; source.write_text(too_long); refused = work/'refused.dis64'
        result = subprocess.run([str(args.limbo.resolve()), '-64b', '-I', str(ROOT/'module'), '-I', str(work), '-o', str(refused), str(source)], env=env, capture_output=True, text=True)
        require(result.returncode > 0 and (not refused.exists() or not refused.stat().st_size) and 'Dis64' in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr, 'overlong name did not refuse cleanly')
        # A different argument type may be a well-formed module, but must not
        # satisfy Command merely because it exports a function named init.
        incompatible = 'implement Wm;include "command.m";Wm:module{init:fn(ctxt:ref Draw->Display,argv:list of string);};init(ctxt:ref Draw->Display,argv:list of string){z:=0;bad:=1/z;}'
        incompatible = compile(incompatible, '-64b', provider)
        proof(incompatible, 'library', 1); proof(incompatible, 'match', 0, caller)
        if args.emu:
            softboot = boot.replace('if(mod == nil) { z := 0; bad := 1/z; }', 'if(mod == nil) return;')
            require(softboot != boot, 'tutorial missing-load guard drift')
            compile(softboot, '-64c', caller)
            for missing in (False, True):
                if missing:provider.unlink()
                for jit in (0, 1):
                    output = BOOT['execute'](command+['-c', str(jit), '-X', 'dis64-calls', '-d', vmcaller])
                    require('native64 scalar module Emuinit: jit='+str(jit)+', exit=""' in output and BOOT['RETIRED'] in output and 'panic:' not in output and 'Broken:' not in output, output)
                    runs += 1
            provider.write_bytes(incompatible)
        extra = CASES['implementation'](sample, 'Wm').replace('argv:list of string);};', 'argv:list of string);extra:fn();};')+'extra(){}'
        compile(extra, '-i', work/'extra.dis')
        source.write_text(extra); refused.unlink(missing_ok=True)
        result = subprocess.run([str(args.limbo.resolve()), '-64b', '-I', str(ROOT/'module'), '-I', str(work), '-o', str(refused), str(source)], env=env, capture_output=True, text=True)
        require(result.returncode > 0 and (not refused.exists() or not refused.stat().st_size) and 'Dis64' in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr, 'extra public export did not refuse cleanly')
        if args.emu:
            vmprovider = '/'+str(provider.relative_to(args.root.resolve()))
            for flags, target in (([], vmcaller), (['-X', 'dis64-calls'], vmprovider)):
                output = BOOT['execute'](command+flags+['-d', target])
                require('panic: loading ' in output and BOOT['RETIRED'] not in output, output)
        print('PASS actual appl/cmd launch witnesses; actual Wm.init hash='+hex(expected)+'; '+str(proofs)+' raw checks; '+str(runs)+' full VM C/JIT roots', flush=True)
        print('Finite Command implementations only; full emuinit/sh/AWM source admission remains separate.')


if __name__ == '__main__':
    main()
