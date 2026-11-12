#!/usr/bin/env python3
"""Consume opt-in Limbo call-site plans; never treat diagnostic JSON as Dis64 metadata."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def checked_run(command, **options):
    try:
        result = subprocess.run(command, **options)
        diagnostic = result.stderr or b''
        if isinstance(diagnostic, bytes): diagnostic = diagnostic.decode(errors='replace')
        if any(marker in diagnostic for marker in ('AddressSanitizer', 'UndefinedBehaviorSanitizer', 'runtime error:')):
            raise RuntimeError('Compiler sanitizer failure:\n'+diagnostic)
        return result
    except subprocess.CalledProcessError as failure:
        diagnostic = failure.stderr or b''
        if isinstance(diagnostic, bytes): diagnostic = diagnostic.decode(errors='replace')
        sys.stderr.write(diagnostic)
        raise


def legacy_instructions(data, with_types=False):
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
    if not with_types: return header, code
    types = {}
    if not 0 <= header[5] <= 65536: raise RuntimeError('Unbounded descriptor table')
    for _ in range(header[5]):
        identity, size, mapsize = number(), number(), number()
        if identity in types or size < 0 or mapsize < 0 or mapsize > (size+31)//32 or at+mapsize > len(data):
            raise RuntimeError('Invalid classic descriptor')
        types[identity] = (size, data[at:at+mapsize]); at += mapsize
    return header, code, types


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


def verify_stores(data, records, diagnostics):
    """Check native packing against independent decoded legacy writes and declared tail kinds."""
    locations = {json.loads(line.split(' ', 1)[1])['id']: json.loads(line.split(' ', 1)[1]) for line in diagnostics.splitlines() if line.startswith('NATIVE64_CALL_PC ')}
    lower = [json.loads(line.split(' ', 1)[1]) for line in diagnostics.splitlines() if line.startswith('NATIVE64_CALL_LOWER ')]
    by_id = {r['id']: r for r in lower}
    if len(by_id) != len(records) or len(lower) != len(records): raise RuntimeError('Missing native store program/refusal')
    _, code, types = legacy_instructions(data, with_types=True)
    for record in records:
        program = by_id[record['id']]
        if not record['accepted']:
            if program['lowered'] or 'stores' in program: raise RuntimeError('Refused type acquired a partial native program')
            continue
        if not program['lowered'] or program['frame_bytes'] != record['frame_bytes'] or program['source_space'] != 'legacy-operands':
            raise RuntimeError('Simple source arguments did not lower: '+str(program))
        location = locations[record['id']]
        fields = set()
        if len(program['stores']) != record['fixed_arguments']+len(record['tail']): raise RuntimeError('Missing field write')
        for store in program['stores']:
            pc = store['legacy_pc']; origin = code[pc]
            if not location['frame_pc'] < pc < location['result_pc'] or origin['m'] is not None:
                raise RuntimeError('Native store was not an actual argument write')
            source_mode = (origin['mode'] >> 3) & 7
            if store['source_mode'] != {0:'mp', 1:'fp', 2:'immediate'}.get(source_mode) or origin['s'] != [store['source_value']]:
                raise RuntimeError('Source operand provenance was rewritten or widened by guessing')
            key = (store['fixed'], store['ordinal'])
            if key in fields: raise RuntimeError('Duplicate native field store')
            fields.add(key)
            if store['fixed']:
                expected_destination = 64+8*store['ordinal']; expected_legacy = 32+4*store['ordinal']; expected_opcode = 41
                if store['opcode'] != expected_opcode or origin['op'] != 41: raise RuntimeError('Fixed reference lost MOVP ownership')
            else:
                slot = record['tail'][store['ordinal']]
                expected_destination = record['fixed_bytes']+slot['offset']; expected_legacy = slot['legacy_offset']
                if slot['kind'] in ('int','fix'): expected_opcode = 138 if origin['op'] == 45 else -1
                elif slot['kind'] == 'big': expected_opcode = origin['op'] if origin['op'] in (118,138) else -1
                elif slot['kind'] == 'real': expected_opcode = origin['op'] if origin['op'] in (46,50,135) else -1
                elif slot['kind'] == 'string': expected_opcode = 41 if origin['op'] == 41 else -1
                else: expected_opcode = -1
                if store['kind'] != slot['kind'] or store['opcode'] != expected_opcode: raise RuntimeError('Wrong canonical value opcode')
            if store['destination'] != expected_destination or store['bytes'] != 8 or origin['d'] != [location['frame_slot'], expected_legacy]:
                raise RuntimeError('Native destination does not match complete typed packing')
        bank = program['bank']
        if bank['evaluation'] != 'per-read': raise RuntimeError('Source storage aliases must not imply value caching')
        source_offsets = {}
        for space_index, space in enumerate(('fp','mp')):
            layout = bank['sources'][space_index]
            reads = [c for c in bank['cells'] if c['source_mode'] == space]
            expected = dict(space=space, policy='word4-ref8', legacy_bytes=0, bytes=0, map=[])
            offsets = {}
            if reads:
                size, roots = types[location['caller_descriptor'] if space=='fp' else 0]
                cursor = 64 if space=='fp' else 0; marks = []
                for offset in range(0,size,4):
                    slot = offset//4
                    root = bool(slot//8<len(roots) and roots[slot//8]&(0x80>>(slot%8)))
                    if space=='fp' and offset<20:
                        if root: raise RuntimeError('Legacy header contains a managed source root')
                        continue
                    if root: cursor = (cursor+7)//8*8; marks.append(cursor)
                    offsets[offset] = cursor; cursor += 8 if root else 4
                size64 = max(8,(cursor+7)//8*8); mapping = [0]*((size64+63)//64)
                for offset in marks: mapping[offset//64] |= 0x80>>((offset//8)%8)
                expected.update(legacy_bytes=size, bytes=size64, map=mapping)
            if layout != expected: raise RuntimeError('Complete native source layout differs from serialized descriptor')
            source_offsets[space] = offsets
        unique = {}; expected_map = [0]*((bank['bytes']+63)//64)
        for store in program['stores']:
            if store['source_mode'] == 'immediate':
                if store['source_cell'] != -1 or store['source_offset'] != 0: raise RuntimeError('Immediate acquired a reference cell')
                continue
            root = store['opcode'] == 41
            width = 4 if root or store['opcode'] in (138,50) else 8
            category = ('string' if store['kind'] == 'string' else 'reference') if root else 'real' if store['opcode'] == 46 else 'long' if width == 8 else 'word'
            key = (store['source_mode'], store['source_value'], width, category)
            for old in unique:
                if old[0] == key[0] and key[1] < old[1]+old[2] and old[1] < key[1]+key[2] and old != key:
                    raise RuntimeError('Overlapping or differently typed source reads were admitted')
            index = unique.setdefault(key, len(unique)); cell = bank['cells'][store['source_cell']]
            if store['source_cell'] != index or store['source_offset'] != 64+index*8 or cell != dict(source_mode=key[0],legacy_offset=key[1],legacy_bytes=width,native_offset=64+index*8,native_bytes=8 if root else width,origin_offset=source_offsets[key[0]][key[1]],**{'class':category}):
                raise RuntimeError('Source relocation lost width, identity or aliasing')
            extent, roots = types[location['caller_descriptor'] if key[0]=='fp' else 0]
            if width == 8 and source_offsets[key[0]][key[1]+4] != cell['origin_offset']+4: raise RuntimeError('Packed scalar source was split')
            if key[1] < (20 if key[0]=='fp' else 0) or key[1]+width > extent: raise RuntimeError('Source escaped original descriptor')
            for offset in range(key[1],key[1]+width,4):
                slot = offset//4
                if bool(slot//8<len(roots) and roots[slot//8]&(0x80>>(slot%8))) != root: raise RuntimeError('Source read has wrong original root role')
            if root: expected_map[(64+index*8)//64] |= 0x80>>index%8
        if len(unique) != len(bank['cells']) or bank['bytes'] != 64+8*len(unique) or bank['map'] != expected_map:
            raise RuntimeError('Source bank extent or full map is wrong')
    return by_id


def emit_program(label, program):
    bank = program['bank']
    lines = ['static const Native64GeneratedStore '+label+'[] = {']
    lines += ['{'+','.join(map(str,(x['opcode'],x['destination'],int(x['fixed']),x['ordinal'],x['source_value'],int(x['source_mode']=='immediate'),x['source_cell'],x['source_offset'])))+'},' for x in program['stores']]
    lines += ['};','static const uchar '+label+'_map[] = {'+','.join(map(str,bank['map']))+'};']
    for index, layout in enumerate(bank['sources']):
        lines += ['static const uchar '+label+'_source_map_'+str(index)+'[] = {'+','.join(map(str,layout['map'] or [0]))+'};']
    lines += ['static const Native64GeneratedSource '+label+'_sources[] = {']
    lines += ['{'+str(layout['bytes'])+','+str(len(layout['map']))+','+label+'_source_map_'+str(index)+'},' for index,layout in enumerate(bank['sources'])]
    lines += ['};','static const Native64GeneratedCell '+label+'_cells[] = {']
    lines += ['{'+','.join(map(str,(0 if c['source_mode']=='fp' else 1,c['origin_offset'],c['native_bytes'],int(c['class'] in ('string','reference')))))+'},' for c in bank['cells']]
    if not bank['cells']: lines += ['{0,0,0,0},']
    lines += ['};','static const Native64GeneratedBank '+label+'_bank = {'+str(bank['bytes'])+','+str(len(bank['cells']))+','+str(len(bank['map']))+','+label+'_map,'+label+'_sources,'+label+'_cells};']
    return lines


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
    checked_run(command+['-o', str(ordinary), str(source)], env=env, check=True, capture_output=True)
    result = checked_run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if ordinary.read_bytes() != traced.read_bytes(): raise RuntimeError('Call-site diagnostics changed legacy output')
    records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in result.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    verify_locations(traced.read_bytes(), records, result.stderr)
    programs = verify_stores(traced.read_bytes(), records, result.stderr)
    repeat = checked_run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if repeat.stderr != result.stderr: raise RuntimeError('Call-site plans are nondeterministic')
    if len(records) != 11 or sum(r['accepted'] for r in records) != 10: raise RuntimeError('Missing accepted/refused source call-site plans: '+str(records))
    if records[-1]['reason'] != 'tail-kind': raise RuntimeError('Opaque reference was admitted without an exact Type witness')
    header = ['/* Generated only from the current compiler\'s typechecked source call plans. */',
              'typedef struct Native64GeneratedStore { unsigned int op, destination, fixed, ordinal; long source; unsigned int immediate; int cell; unsigned int source_offset; } Native64GeneratedStore;',
              'typedef struct Native64GeneratedSource { unsigned int bytes, mapbytes; const uchar *map; } Native64GeneratedSource;',
              'typedef struct Native64GeneratedCell { unsigned int space, offset, bytes, root; } Native64GeneratedCell;',
              'typedef struct Native64GeneratedBank { unsigned int bytes, count, mapbytes; const uchar *map; const Native64GeneratedSource *sources; const Native64GeneratedCell *cells; } Native64GeneratedBank;']
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
            header += emit_program('callstores_'+label, programs[record['id']])
    if records[8]['tail'] or records[8]['frame_bytes'] != 72: raise RuntimeError('Empty tail layout failed')
    if [s['kind'] for s in records[9]['tail']] != ['int', 'big', 'real', 'string']: raise RuntimeError('Numeric source kinds lost')
    for flag in ('-64', '-64m'):
        refused = work/('refused'+flag+'.dis64')
        attempt = checked_run(command+[flag, '-Dq', '-o', str(refused), str(source)], env=env, capture_output=True, text=True)
        if attempt.returncode != 1 or refused.exists(): raise RuntimeError('Diagnostic plan bypassed Dis64 admission')
    # Verify the runtime's private descriptor-count boundary from actual source,
    # while keeping the legacy producer usable for the refused call as well.
    for count in (4096, 4097):
        boundary = work/('boundary'+str(count)+'.b')
        boundary.write_text('implement Boundary; include "sys.m"; Boundary: module { init: fn(); }; '
                            'init() { sys := load Sys Sys->PATH; number := 1; sys->sprint('+json.dumps('%d'*count)+', '+
                            ', '.join(['number']*count)+'); }')
        ordinary_boundary = work/'boundary.dis'
        traced_boundary = work/'boundary-trace.dis'
        checked_run(command+['-o', str(ordinary_boundary), str(boundary)], env=env, check=True, capture_output=True)
        output = checked_run(command+['-Dq', '-o', str(traced_boundary), str(boundary)], env=env, check=True, capture_output=True, text=True)
        if ordinary_boundary.read_bytes() != traced_boundary.read_bytes(): raise RuntimeError('Boundary diagnostics changed legacy output')
        plans = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
        if len(plans) != 1 or plans[0]['accepted'] != (count == 4096): raise RuntimeError('Tail-count boundary was not enforced')
        plan = plans[0]
        verify_locations(traced_boundary.read_bytes(), plans, output.stderr)
        verify_stores(traced_boundary.read_bytes(), plans, output.stderr)
        if count == 4097:
            if plan['reason'] != 'tail-count': raise RuntimeError('Incorrect boundary refusal')
        elif len(plan['tail']) != count or plan['frame_bytes'] != 72+count*8 or plan['map'] != [0, 0x80]+[0]*512:
            raise RuntimeError('Maximum admitted call-site map was truncated')
    nested = work/'nested.b'
    nested.write_text('implement Nested; include "sys.m"; Nested: module { init: fn(); }; '
                      'answer: string; init() { sys := load Sys Sys->PATH; answer = sys->sprint("%s", sys->sprint("nested")); }')
    nested_plain = work/'nested.dis'; nested_trace = work/'nested-trace.dis'
    checked_run(command+['-o', str(nested_plain), str(nested)], env=env, check=True, capture_output=True)
    output = checked_run(command+['-Dq', '-o', str(nested_trace), str(nested)], env=env, check=True, capture_output=True, text=True)
    if nested_plain.read_bytes() != nested_trace.read_bytes(): raise RuntimeError('Nested provenance changed legacy output')
    nested_records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    positions = verify_locations(nested_trace.read_bytes(), nested_records, output.stderr)
    nested_lower = {r['id']:r for r in (json.loads(line.split(' ',1)[1]) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL_LOWER '))}
    if nested_lower[nested_records[0]['id']]['lowered'] or 'stores' in nested_lower[nested_records[0]['id']] or not nested_lower[nested_records[1]['id']]['lowered']:
        raise RuntimeError('Nested temporary cleanup was silently dropped from lowering')
    outer, inner = [positions[r['id']] for r in nested_records]
    if not inner['call_pc'] < outer['frame_pc'] or {outer['result_space'], inner['result_space']} != {'fp', 'mp'}:
        raise RuntimeError('Nested call provenance confused source order and execution order')
    asynchronous = work/'asynchronous.b'
    asynchronous.write_text('implement Asynchronous; Async: module { log: fn(fmt: string, *); }; '
                             'Asynchronous: module { init: fn(); }; init() { other := load Async "$Async"; spawn other->log("spawn"); }')
    async_plain = work/'asynchronous.dis'; async_trace = work/'asynchronous-trace.dis'
    checked_run(command+['-o', str(async_plain), str(asynchronous)], env=env, check=True, capture_output=True)
    output = checked_run(command+['-Dq', '-o', str(async_trace), str(asynchronous)], env=env, check=True, capture_output=True, text=True)
    if async_plain.read_bytes() != async_trace.read_bytes(): raise RuntimeError('Spawn diagnostics changed legacy output')
    async_sites = [json.loads(line.removeprefix('NATIVE64_CALL_PC ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL_PC ')]
    if len(async_sites) != 1 or not async_sites[0]['located'] or async_sites[0]['linked'] or async_sites[0]['result_pc'] != -1:
        raise RuntimeError('Void/spawn transport was mistaken for a synchronous result call')
    _, async_code = legacy_instructions(async_trace.read_bytes())
    if async_code[async_sites[0]['call_pc']]['op'] != 10: raise RuntimeError('Spawn location was not the serialized MSPAWN')
    output = checked_run(command+['-Dq', str(nested), str(asynchronous)], cwd=work, env=env, check=True, capture_output=True, text=True)
    batch_records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    batch_sites = [json.loads(line.removeprefix('NATIVE64_CALL_PC ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL_PC ')]
    if len(batch_records) != 3 or len({r['id'] for r in batch_records}) != 3 or {s['module'] for s in batch_sites} != {'Nested', 'Asynchronous'}:
        raise RuntimeError('Module-local PC notes leaked or identities repeated in a multi-file compile')
    extra_programs = {}
    extra_sources = {
        'literal': 'sys->sprint("%d|%s", -1, "ok");',
        'alias': 'sys->sprint("%d|%s", -1, "%d|%s");',
        'wide': 'wide := big -17; fraction := -1.25; sys->sprint("%bd|%g", wide, fraction);',
        'castalias': 'wide := big -17; sys->sprint("%bd|%g", wide, real wide);',
        'computed': 'number := -1; sys->sprint("%d", number+1);',
    }
    for label, body in extra_sources.items():
        probe = work/(label+'.b')
        probe.write_text('implement Probe; include "sys.m"; Probe: module { init: fn(); }; init() { sys := load Sys Sys->PATH; '+body+' }')
        plain = work/(label+'.dis'); traced_probe = work/(label+'-trace.dis')
        checked_run(command+['-o', str(plain), str(probe)], env=env, check=True, capture_output=True)
        output = checked_run(command+['-Dq', '-o', str(traced_probe), str(probe)], env=env, check=True, capture_output=True, text=True)
        if plain.read_bytes() != traced_probe.read_bytes(): raise RuntimeError('Store lowering changed legacy output')
        probe_records = [json.loads(line.split(' ', 1)[1]) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
        verify_locations(traced_probe.read_bytes(), probe_records, output.stderr)
        if label == 'computed':
            refused = [json.loads(line.split(' ',1)[1]) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL_LOWER ')]
            if len(refused) != 1 or refused[0]['lowered'] or 'stores' in refused[0]: raise RuntimeError('Unsupported arithmetic emitted a partial store program')
        else:
            extra_programs[label] = verify_stores(traced_probe.read_bytes(), probe_records, output.stderr)[probe_records[0]['id']]
    if len(extra_programs['castalias']['bank']['cells']) != 2: raise RuntimeError('Big reads and conversions failed to share one typed source')
    if len(extra_programs['alias']['bank']['cells']) != 1: raise RuntimeError('Identical String sources lost their shared cell')
    for label, program in extra_programs.items(): header += emit_program('call'+label+'stores', program)
    header += emit_program('callmixedstores', programs[records[9]['id']])
    (work/'callsites-native64.h').write_text('\n'.join(header)+'\n')
    print('PASS source call-site plans: four Sys operations, 2/150/4096 slots, 4097 refusal, empty/mixed tails, complete maps, opaque-reference refusal, deterministic diagnostics, unchanged legacy bytecode and preserved Dis64 refusal, byte-decoded final PCs, nested FP/MP results and canonical native store programs', flush=True)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='native64-callsites-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        generate(args.limbo.resolve(), work, dict(os.environ, TMPDIR=str(work)))


if __name__ == '__main__': main()
