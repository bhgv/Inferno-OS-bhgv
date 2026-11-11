#!/usr/bin/env python3
"""Consume opt-in Limbo call-site plans; never treat diagnostic JSON as Dis64 metadata."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def legacy_instructions(data):
    """Independent classic Dis code reader; no diagnostic or Dis64 parser is reused."""
    at = 0
    def number():
        nonlocal at
        first = data[at]; at += 1
        length, bits = (1, 7) if first < 128 else (2, 14) if first < 192 else (4, 30)
        value = first & (0x7f if length == 1 else 0x3f)
        for _ in range(length-1): value = (value << 8) | data[at]; at += 1
        return value-(1 << bits) if value & (1 << (bits-1)) else value
    header = [number() for _ in range(9)]
    if header[0] != 819248 or not 0 <= header[3] <= 1024*1024: raise RuntimeError('Not a bounded classic Dis image')
    code = []
    def operand(mode):
        if mode == 3: return None
        if mode in (0, 1, 2): return [number()]
        if mode in (4, 5): return [number(), number()]
        raise RuntimeError('Unsupported classic address mode')
    for _ in range(header[3]):
        op, mode = data[at:at+2]; at += 2
        middle = number() if mode & 0xc0 else None
        source = operand((mode >> 3) & 7); destination = operand(mode & 7)
        code.append(dict(op=op, mode=mode, m=middle, s=source, d=destination))
    return header, code


def verify_locations(data, records, diagnostics):
    sites = [json.loads(line.removeprefix('NATIVE64_CALL_PC ')) for line in diagnostics.splitlines() if line.startswith('NATIVE64_CALL_PC ')]
    by_id = {site['id']: site for site in sites}
    if len(sites) != len(records) or len(by_id) != len(records) or set(by_id) != {r['id'] for r in records}:
        raise RuntimeError('Call-site identities/locations were lost or duplicated')
    header, code = legacy_instructions(data)
    spaces = {0: 'mp', 1: 'fp', 4: 'mp-indirect', 5: 'fp-indirect'}
    for record in records:
        site = by_id[record['id']]
        if site['pc_space'] != 'legacy-dis' or not site['located'] or not site['linked']:
            raise RuntimeError('Ordinary Sys call was not located after optimization: '+str(site))
        if site['caller_pc'] != header[7] or site['caller_descriptor'] != header[8]: raise RuntimeError('Caller identity does not match the serialized init entry/frame')
        frame, result, call = [code[site[k]] for k in ('frame_pc', 'result_pc', 'call_pc')]
        if (frame['op'], result['op'], call['op']) != (5, 39, 9): raise RuntimeError('PCs do not identify actual FRAME/LEA/MCALL bytes')
        if not 0 <= site['frame_descriptor'] < header[5] or frame['s'] != [site['frame_descriptor']]: raise RuntimeError('Wrong actual descriptor ordinal')
        if frame['d'] != [site['frame_slot']] or result['d'] != [site['frame_slot'], 16] or call['s'] != [site['frame_slot']]:
            raise RuntimeError('Prepared frame register/result-header relation was guessed')
        if site['binding'] != call['m'] or call['d'] != [site['module_offset']] or spaces.get(call['mode'] & 7) != site['module_space']:
            raise RuntimeError('Import/module operands do not match serialized code')
        actual = result['s']
        if spaces.get((result['mode'] >> 3) & 7) != site['result_space'] or actual[0] != site['result_offset'] or (len(actual) == 2 and actual[1] != site['result_indirect']):
            raise RuntimeError('Result address is not the actual legacy operand')
    return by_id


def generate(limbo, work, env):
    source = work/'callsites.b'
    calls = []
    for name in ('sprint', 'aprint', 'print', 'fprint'):
        for count in (2, 150):
            kinds = ['string' if i in (1, 70, 149) else 'int' for i in range(count)]
            fmt = '|'.join('%s' if k == 'string' else '%d' for k in kinds)
            values = ', '.join('text' if k == 'string' else 'number' for k in kinds)
            calls.append('sys->'+name+'('+('fd, ' if name == 'fprint' else '')+json.dumps(fmt)+', '+values+');')
    calls += ['sys->sprint("empty");', 'sys->sprint("%d|%bd|%g|%s", number, big number, real number, text);',
              'sys->sprint("%x", array[1] of byte);']
    source.write_text('implement CallSites;\ninclude "sys.m";\nCallSites: module { init: fn(); };\n'
                      'init() { sys := load Sys Sys->PATH; number := -1; text := "ok"; fd := sys->fildes(1);\n'+
                      '\n'.join(calls)+'\n}\n')
    command = [str(limbo), '-I', str(ROOT/'module')]
    ordinary = work/'ordinary.dis'; traced = work/'traced.dis'
    subprocess.run(command+['-o', str(ordinary), str(source)], env=env, check=True, capture_output=True)
    result = subprocess.run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if ordinary.read_bytes() != traced.read_bytes(): raise RuntimeError('Call-site diagnostics changed legacy output')
    records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in result.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    verify_locations(traced.read_bytes(), records, result.stderr)
    repeat = subprocess.run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if repeat.stderr != result.stderr: raise RuntimeError('Call-site plans are nondeterministic')
    if len(records) != 11 or sum(r['accepted'] for r in records) != 10: raise RuntimeError('Missing accepted/refused source call-site plans: '+str(records))
    if records[-1]['reason'] != 'tail-kind': raise RuntimeError('Opaque reference was admitted without an exact Type witness')
    header = ['/* Generated only from the current compiler\'s typechecked source call plans. */']
    for operation, name in enumerate(('sprint', 'aprint', 'print', 'fprint')):
        for index, count in enumerate((2, 150)):
            record = records[operation*2+index]
            if record['callee'] != 'Sys_'+name or len(record['tail']) != count: raise RuntimeError('Call-site identity mismatch')
            if record['result'] != ('string' if name == 'sprint' else 'array-byte' if name == 'aprint' else 'int') or record['fixed_arguments'] != (2 if name == 'fprint' else 1):
                raise RuntimeError('Original function result/fixed argument identity was lost')
            fixed = 80 if name == 'fprint' else 72
            expected = [0]*((fixed+count*8+63)//64)
            for offset in ([64, 72] if name == 'fprint' else [64]): expected[offset//64] |= 0x80>>((offset//8)%8)
            for i, slot in enumerate(record['tail']):
                kind = 'string' if i in (1, 70, 149) else 'int'
                if slot['kind'] != kind or slot['offset'] != i*8 or slot['bytes'] != 8 or slot['legacy_offset'] != (40 if name == 'fprint' else 36)+i*4:
                    raise RuntimeError('Incorrect actual argument type/slot')
                if kind == 'string': expected[(fixed+i*8)//64] |= 0x80>>(((fixed+i*8)//8)%8)
            if record['fixed_bytes'] != fixed or record['frame_bytes'] != fixed+count*8 or record['map'] != expected:
                raise RuntimeError('Incorrect compiler-produced complete frame geometry')
            label = str(operation)+'_'+str(count)
            header += ['static const uchar callmap_'+label+'[] = {'+','.join(map(str, record['map']))+'};',
                       'static const UWORD callsignature_'+label+' = '+str(record['signature'])+'U;',
                       'static Native64TailSlot callslots_'+label+'[] = {']
            header += ['{'+('Native64TailRef' if slot['kind'] == 'string' else 'Native64TailInt')+','+str(slot['offset'])+',8,'+('&Tstring' if slot['kind'] == 'string' else 'nil')+'},' for slot in record['tail']]
            header += ['};']
    if records[8]['tail'] or records[8]['frame_bytes'] != 72: raise RuntimeError('Empty tail layout failed')
    if [s['kind'] for s in records[9]['tail']] != ['int', 'big', 'real', 'string']: raise RuntimeError('Numeric source kinds lost')
    for flag in ('-64', '-64m'):
        refused = work/('refused'+flag+'.dis64')
        attempt = subprocess.run(command+[flag, '-Dq', '-o', str(refused), str(source)], env=env, capture_output=True, text=True)
        if attempt.returncode == 0 or refused.exists(): raise RuntimeError('Diagnostic plan bypassed Dis64 admission')
    # Verify the runtime's private descriptor-count boundary from actual source,
    # while keeping the legacy producer usable for the refused call as well.
    for count in (4096, 4097):
        boundary = work/('boundary'+str(count)+'.b')
        boundary.write_text('implement Boundary; include "sys.m"; Boundary: module { init: fn(); }; '
                            'init() { sys := load Sys Sys->PATH; number := 1; sys->sprint('+json.dumps('%d'*count)+', '+
                            ', '.join(['number']*count)+'); }')
        ordinary_boundary = work/'boundary.dis'
        traced_boundary = work/'boundary-trace.dis'
        subprocess.run(command+['-o', str(ordinary_boundary), str(boundary)], env=env, check=True, capture_output=True)
        output = subprocess.run(command+['-Dq', '-o', str(traced_boundary), str(boundary)], env=env, check=True, capture_output=True, text=True)
        if ordinary_boundary.read_bytes() != traced_boundary.read_bytes(): raise RuntimeError('Boundary diagnostics changed legacy output')
        plans = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
        if len(plans) != 1 or plans[0]['accepted'] != (count == 4096): raise RuntimeError('Tail-count boundary was not enforced')
        plan = plans[0]
        verify_locations(traced_boundary.read_bytes(), plans, output.stderr)
        if count == 4097:
            if plan['reason'] != 'tail-count': raise RuntimeError('Incorrect boundary refusal')
        elif len(plan['tail']) != count or plan['frame_bytes'] != 72+count*8 or plan['map'] != [0, 0x80]+[0]*512:
            raise RuntimeError('Maximum admitted call-site map was truncated')
    nested = work/'nested.b'
    nested.write_text('implement Nested; include "sys.m"; Nested: module { init: fn(); }; '
                      'answer: string; init() { sys := load Sys Sys->PATH; answer = sys->sprint("%s", sys->sprint("nested")); }')
    nested_plain = work/'nested.dis'; nested_trace = work/'nested-trace.dis'
    subprocess.run(command+['-o', str(nested_plain), str(nested)], env=env, check=True, capture_output=True)
    output = subprocess.run(command+['-Dq', '-o', str(nested_trace), str(nested)], env=env, check=True, capture_output=True, text=True)
    if nested_plain.read_bytes() != nested_trace.read_bytes(): raise RuntimeError('Nested provenance changed legacy output')
    nested_records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    positions = verify_locations(nested_trace.read_bytes(), nested_records, output.stderr)
    outer, inner = [positions[r['id']] for r in nested_records]
    if not inner['call_pc'] < outer['frame_pc'] or {outer['result_space'], inner['result_space']} != {'fp', 'mp'}:
        raise RuntimeError('Nested call provenance confused source order and execution order')
    asynchronous = work/'asynchronous.b'
    asynchronous.write_text('implement Asynchronous; Async: module { log: fn(fmt: string, *); }; '
                             'Asynchronous: module { init: fn(); }; init() { other := load Async "$Async"; spawn other->log("spawn"); }')
    async_plain = work/'asynchronous.dis'; async_trace = work/'asynchronous-trace.dis'
    subprocess.run(command+['-o', str(async_plain), str(asynchronous)], env=env, check=True, capture_output=True)
    output = subprocess.run(command+['-Dq', '-o', str(async_trace), str(asynchronous)], env=env, check=True, capture_output=True, text=True)
    if async_plain.read_bytes() != async_trace.read_bytes(): raise RuntimeError('Spawn diagnostics changed legacy output')
    async_sites = [json.loads(line.removeprefix('NATIVE64_CALL_PC ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL_PC ')]
    if len(async_sites) != 1 or not async_sites[0]['located'] or async_sites[0]['linked'] or async_sites[0]['result_pc'] != -1:
        raise RuntimeError('Void/spawn transport was mistaken for a synchronous result call')
    _, async_code = legacy_instructions(async_trace.read_bytes())
    if async_code[async_sites[0]['call_pc']]['op'] != 10: raise RuntimeError('Spawn location was not the serialized MSPAWN')
    output = subprocess.run(command+['-Dq', str(nested), str(asynchronous)], cwd=work, env=env, check=True, capture_output=True, text=True)
    batch_records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    batch_sites = [json.loads(line.removeprefix('NATIVE64_CALL_PC ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL_PC ')]
    if len(batch_records) != 3 or len({r['id'] for r in batch_records}) != 3 or {s['module'] for s in batch_sites} != {'Nested', 'Asynchronous'}:
        raise RuntimeError('Module-local PC notes leaked or identities repeated in a multi-file compile')
    (work/'callsites-native64.h').write_text('\n'.join(header)+'\n')
    print('PASS source call-site plans: four Sys operations, 2/150/4096 slots, 4097 refusal, empty/mixed tails, complete maps, opaque-reference refusal, deterministic diagnostics, unchanged legacy bytecode and preserved Dis64 refusal, byte-decoded final PCs and nested FP/MP result locations', flush=True)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='native64-callsites-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        generate(args.limbo.resolve(), work, dict(os.environ, TMPDIR=str(work)))


if __name__ == '__main__': main()
