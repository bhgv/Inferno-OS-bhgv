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


def legacy_instructions(data, with_types=False, with_data=False, with_exports=False, with_imports=False):
    """Independent classic Dis code reader; no diagnostic or Dis64 parser is reused."""
    if with_imports: with_exports=True
    if with_exports: with_types=with_data=True
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
    if not with_data: return header, code, types
    initial = {}
    while True:
        tag = data[at]; at += 1
        if tag == 0: break
        kind, count = tag>>4, tag&15
        if not count: count = number()
        offset = number()
        if count < 0 or offset < 0: raise RuntimeError('Invalid classic data record')
        if kind == 3:
            if at+count > len(data): raise RuntimeError('Truncated classic String')
            initial[offset] = dict(kind='string',bytes=list(data[at:at+count])); at += count
        elif kind in (1,2,4,8):
            width = {1:1,2:4,4:8,8:8}[kind]
            if count > (len(data)-at)//width: raise RuntimeError('Truncated classic scalar data')
            for i in range(count):
                value = int.from_bytes(data[at:at+width],'big'); at += width
                initial[offset+i*width] = dict(kind={1:'byte',2:'word',4:'real',8:'long'}[kind],high=value>>32,low=value&0xffffffff)
        else: raise RuntimeError('Non-flat classic initializer in this bounded source gate')
    if with_exports:
        def name():
            nonlocal at
            end=data.index(0,at);value=data[at:end].decode('utf-8');at=end+1;return value
        module=name();exports=[]
        assert 0<=header[6]<=65536
        for _ in range(header[6]):
            pc,descriptor=number(),number()
            assert at+4<=len(data)
            signature=int.from_bytes(data[at:at+4],'big');at+=4
            exports.append(dict(pc=pc,descriptor=descriptor,signature=signature,name=name()))
        if with_imports:
            assert header[1]&64
            count=number();assert 0<=count<=65536;tables=[]
            for _ in range(count):
                n=number();assert 0<=n<=65536;table=[]
                for _ in range(n):
                    assert at+4<=len(data)
                    signature=int.from_bytes(data[at:at+4],'big');at+=4
                    table.append(dict(signature=signature,name=name()))
                tables.append(table)
            assert number()==0
            return header,code,types,initial,module,exports,tables
        return header,code,types,initial,module,exports
    return header, code, types, initial


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
    header, code, types, initial = legacy_instructions(data, with_types=True, with_data=True)
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
            expected = dict(space=space, policy='typed-word4-ref8', legacy_bytes=0, bytes=0, map=[], leaves=[])
            offsets = {}
            if reads or (space=='mp' and any((c.get('setup') or {}).get('source_mode')=='mp' for c in bank['cells'])):
                size, roots = types[location['caller_descriptor'] if space=='fp' else 0]
                size64=layout['bytes']; mapping=[0]*((size64+63)//64)
                if not 0<=size64<=1024*1024 or size64%8: raise RuntimeError('Invalid typed source extent')
                marks=set(); occupied=set(); previous=0
                for old,new,oldbytes,width,kind in layout['leaves']:
                    if old<previous or oldbytes<=0 or old+oldbytes>size or new<(64 if space=='fp' else 0) or width<=0 or new+width>size64:
                        raise RuntimeError('Typed leaf escapes source storage')
                    previous=old+oldbytes
                    if occupied.intersection(range(new,new+width)): raise RuntimeError('Typed target fields overlap')
                    occupied.update(range(new,new+width))
                    root=kind>=0 and oldbytes==4 and width==8
                    if root:
                        if old%4 or new%8: raise RuntimeError('Misaligned typed reference')
                        marks.add(old//4); mapping[new//64] |= 0x80>>(new//8%8)
                    elif kind>=0 and (oldbytes!=width or width not in (1,4,8) or new%width):
                        raise RuntimeError('Invalid typed scalar width/alignment')
                    if kind>=0:
                        for byte in range(0,oldbytes-3,4):
                            if (old+byte)%4==0: offsets[old+byte]=new+byte
                actual={i*8+b for i,value in enumerate(roots) for b in range(8) if value&(0x80>>b)}
                if marks!=actual: raise RuntimeError('Typed leaves disagree with serialized descriptor roots')
                expected.update(legacy_bytes=size,bytes=size64,map=mapping,leaves=layout['leaves'])
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
            if store['source_cell'] != index or store['source_offset'] != 64+index*8 or cell != dict(source_mode=key[0],legacy_offset=key[1],legacy_bytes=width,native_offset=64+index*8,native_bytes=8 if root else width,origin_offset=source_offsets[key[0]][key[1]],initial=cell['initial'],setup=cell['setup'],aggregate=cell['aggregate'],**{'class':category}):
                raise RuntimeError('Source relocation lost width, identity or aliasing')
            setup = cell['setup']
            if setup is not None:
                pc = setup['legacy_pc']; definition = code[pc]
                widths = {45:4,41:4,118:8,46:8,138:8,50:8,135:8}
                allowed = {'word':(45,), 'string':(41,), 'long':(118,138), 'real':(46,50,135)}
                aggregate=cell['aggregate']
                if aggregate is not None:
                    if definition['op']!=aggregate['opcode'] or definition['m']!=aggregate['descriptor'] or definition['s']!=[aggregate['source']] or definition['d']!=[aggregate['destination']] or (definition['mode']>>3)&7!=0 or definition['mode']&7!=1:
                        raise RuntimeError('Aggregate projection lost its actual copy instruction')
                    if definition['op']!=43: raise RuntimeError('Non-MOVMP became a projected definition')
                    extent,copyroots=types[aggregate['descriptor']]
                    relative=key[1]-aggregate['destination']
                    if relative<0 or relative+width>extent: raise RuntimeError('Aggregate field escapes copied descriptor')
                    for offset in range(relative,relative+width,4):
                        bit=offset//4
                        if bool(bit//8<len(copyroots) and copyroots[bit//8]&(0x80>>(bit%8)))!=root: raise RuntimeError('Aggregate projection changes root role')
                    if setup['source_value']!=aggregate['source']+relative: raise RuntimeError('Aggregate projection changed source field')
                    definition=dict(op=setup['opcode'],mode=1,s=[setup['source_value']],d=[key[1]],m=None)
                if header[1]&32 or key[0] != 'fp' or not location['caller_pc'] <= pc < location['frame_pc']:
                    raise RuntimeError('FP setup has no synchronous exception-free suffix')
                if definition['op'] != setup['opcode'] or setup['opcode'] not in allowed.get(category,()) or definition['d'] != [key[1]] or definition['mode']&7 != 1 or widths[definition['op']] != width:
                    raise RuntimeError('FP setup is not the actual exact-width definition')
                mode = (definition['mode']>>3)&7
                if setup['source_mode'] != {0:'mp',2:'immediate'}.get(mode) or definition['s'] != [setup['source_value']]:
                    raise RuntimeError('FP setup source provenance was guessed')
                for between in code[pc+(aggregate is not None):location['frame_pc']]:
                    if between['op'] not in widths or between['m'] is not None or between['mode']&7 != 1:
                        raise RuntimeError('FP setup crosses a call, alias or unknown write')
                for between in code[pc+1:location['frame_pc']]:
                    if between['d'][0] < key[1]+width and key[1] < between['d'][0]+widths[between['op']]:
                        raise RuntimeError('FP setup was overwritten')
                for branch in code:
                    if branch['op'] in (1,2,3,14,144,159): raise RuntimeError('FP setup has an untracked dispatch entry')
                    if branch['op'] in (4,6,13) or 87<=branch['op']<=110 or 129<=branch['op']<=134:
                        if branch['d'] and pc < branch['d'][0] <= location['frame_pc']: raise RuntimeError('Branch enters after the FP definition')
                if mode == 2:
                    if setup['source_offset'] != 0 or setup['source_bytes'] != 4: raise RuntimeError('Immediate definition acquired a native source address')
                    expected_input = dict(kind='word',high=0,low=setup['source_value']&0xffffffff)
                else:
                    input_width = 4 if definition['op'] in (41,45,138,50) else 8
                    expected_offset = source_offsets['mp'][setup['source_value']]
                    if setup['source_offset'] != expected_offset or setup['source_bytes'] != (8 if definition['op']==41 else input_width): raise RuntimeError('Definition input relocation lost its exact source span')
                    if input_width == 8 and source_offsets['mp'][setup['source_value']+4] != expected_offset+4: raise RuntimeError('Definition input scalar was split')
                    extent, roots = types[0]; offset = setup['source_value']
                    if offset<0 or offset%4 or offset+input_width>extent: raise RuntimeError('FP setup input escapes MP')
                    for byte in range(offset,offset+input_width,4):
                        bit = byte//4
                        if bool(bit//8<len(roots) and roots[bit//8]&(0x80>>(bit%8))) != (definition['op']==41): raise RuntimeError('FP setup input root role is wrong')
                    expected_input = initial.get(offset)
                    if setup['input'] == dict(kind='nil-string') and definition['op']==41 and expected_input is None: expected_input = setup['input']
                if setup['input'] is not None and setup['input'] != expected_input: raise RuntimeError('FP setup initializer differs from actual operand/data')
            if cell['initial'] is not None:
                expected_seed = initial.get(key[1])
                if cell['initial']['kind'] == 'nil-string':
                    if category != 'string' or expected_seed is not None: raise RuntimeError('Nil String initializer overwrites actual data')
                    expected_seed = dict(kind='nil-string')
                elif cell['initial']['kind'] != category: raise RuntimeError('Initializer lost its exact scalar/reference kind')
                if key[0] != 'mp' or cell['initial'] != expected_seed:
                    raise RuntimeError('Native source initializer differs from actual classic data bytes')
            extent, roots = types[location['caller_descriptor'] if key[0]=='fp' else 0]
            if width == 8 and source_offsets[key[0]][key[1]+4] != cell['origin_offset']+4: raise RuntimeError('Packed scalar source was split')
            if key[1] < (20 if key[0]=='fp' else 0) or key[1]+width > extent: raise RuntimeError('Source escaped original descriptor')
            for offset in range(key[1],key[1]+width,4):
                slot = offset//4
                if bool(slot//8<len(roots) and roots[slot//8]&(0x80>>(slot%8))) != root: raise RuntimeError('Source read has wrong original root role')
            if root: expected_map[(64+index*8)//64] |= 0x80>>index%8
        expected_order = sorted((i for i,c in enumerate(bank['cells']) if c['setup'] is not None),key=lambda i:bank['cells'][i]['setup']['legacy_pc'])
        if bank['preparation'] != expected_order:
            raise RuntimeError('Local preparation order lost instruction identity')
        shared={}
        for index in expected_order:
            cell=bank['cells'][index]; pc=cell['setup']['legacy_pc']
            if pc in shared and (cell['aggregate'] is None or cell['aggregate']!=shared[pc]):
                raise RuntimeError('Different definitions share a setup PC')
            shared[pc]=cell['aggregate']
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
    lines += ['};']
    for index, cell in enumerate(bank['cells']):
        seed = cell['initial'] if cell['initial'] is not None else (cell['setup'] or {}).get('input'); payload = seed['bytes'] if seed and seed['kind']=='string' else []
        lines += ['static const uchar '+label+'_seed_'+str(index)+'[] = {'+','.join(map(str,payload or [0]))+'};']
    lines += ['static const Native64GeneratedCell '+label+'_cells[] = {']
    for index, cell in enumerate(bank['cells']):
        seed = cell['initial'] if cell['initial'] is not None else (cell['setup'] or {}).get('input'); kind = 0 if seed is None else {'string':1,'word':2,'long':3,'real':4,'nil-string':5}[seed['kind']]
        values = [0 if cell['source_mode']=='fp' else 1,cell['origin_offset'],cell['native_bytes'],int(cell['class'] in ('string','reference')),kind,
                  len(seed['bytes']) if kind==1 else 0,seed['high'] if kind in (2,3,4) else 0,seed['low'] if kind in (2,3,4) else 0]
        lines += ['{'+','.join(str(x)+'U' for x in values)+','+label+'_seed_'+str(index)+','+str(cell['setup']['opcode'] if cell['setup'] else 0)+','+str(cell['setup']['legacy_pc'] if cell['setup'] else 0)+','+str(int(cell['setup'] is not None and cell['setup']['source_mode']=='mp'))+','+str(cell['setup']['source_offset'] if cell['setup'] else 0)+','+str(cell['setup']['source_bytes'] if cell['setup'] else 0)+'},']
    if not bank['cells']: lines += ['{0,0,0,0,0,0,0,0,0,0,0,0,0,0},']
    lines += ['};','static const unsigned int '+label+'_order[] = {'+','.join(map(str,bank['preparation'] or [0]))+'};','static const Native64GeneratedBank '+label+'_bank = {'+str(bank['bytes'])+','+str(len(bank['cells']))+','+str(len(bank['map']))+','+label+'_map,'+label+'_sources,'+label+'_cells,'+str(len(bank['preparation']))+','+label+'_order};']
    return lines


def generate(limbo, work, env):
    source = work/'callsites.b'
    calls = []
    for name in ('sprint', 'aprint', 'print', 'fprint'):
        for count in (2, 150):
            kinds = ['string' if i in (1, 70, 149) else 'int' for i in range(count)]
            fmt = '|'.join('%s' if k == 'string' else '%d' for k in kinds)
            values = ', '.join('text' if k == 'string' else 'number' for k in kinds)
            calls.append('number = -1; text = "ok"; sys->'+name+'('+('fd, ' if name == 'fprint' else '')+json.dumps(fmt)+', '+values+');')
    calls += ['sys->sprint("empty");', 'sys->sprint("%d|%bd|%g|%s", number, big number, real number, text);',
              'sys->sprint("%x", array[1] of byte);']
    source.write_text('implement CallSites;\ninclude "sys.m";\nCallSites: module { init: fn(); };\n'
                      'init() { sys := load Sys Sys->PATH; number := -1; text := "ok"; fd := sys->fildes(1);\n'+
                      '\n'.join(calls)+'\n}\n')
    command = [str(limbo), '-I', str(ROOT/'module'), '-I', str(work)]
    ordinary = work/'ordinary.dis'; traced = work/'traced.dis'
    checked_run(command+['-o', str(ordinary), str(source)], env=env, check=True, capture_output=True)
    result = checked_run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if ordinary.read_bytes() != traced.read_bytes(): raise RuntimeError('Call-site diagnostics changed legacy output')
    records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in result.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    verify_locations(traced.read_bytes(), records, result.stderr)
    programs = verify_stores(traced.read_bytes(), records, result.stderr)
    for record in records[:8]:
        for cell in programs[record['id']]['bank']['cells']:
            if cell['source_mode']=='fp' and cell['class'] in ('word','string') and (cell['setup'] is None or cell['setup']['input'] is None):
                raise RuntimeError('Fixture locals still require an unproved C source value')
    # A wrong definition identity or value must fail the independent bytecode oracle.
    for fault in ('pc','opcode','value','order'):
        altered = json.loads(json.dumps(programs[records[0]['id']]))
        cell = next(c for c in altered['bank']['cells'] if c['source_mode']=='fp' and c['class']=='word')
        if fault=='pc': cell['setup']['legacy_pc'] = verify_locations(traced.read_bytes(), records, result.stderr)[records[0]['id']]['frame_pc']
        elif fault=='opcode': cell['setup']['opcode'] = 41
        elif fault=='value': cell['setup']['input']['low'] ^= 1
        else: altered['bank']['preparation'] = list(reversed(altered['bank']['preparation']))
        changed = '\n'.join('NATIVE64_CALL_LOWER '+json.dumps(altered) if line.startswith('NATIVE64_CALL_LOWER ') and json.loads(line.split(' ',1)[1])['id']==altered['id'] else line for line in result.stderr.splitlines())
        try: verify_stores(traced.read_bytes(), records, changed)
        except RuntimeError: pass
        else: raise RuntimeError('FP definition oracle accepted corrupt '+fault)
    repeat = checked_run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if repeat.stderr != result.stderr: raise RuntimeError('Call-site plans are nondeterministic')
    if len(records) != 11 or sum(r['accepted'] for r in records) != 10: raise RuntimeError('Missing accepted/refused source call-site plans: '+str(records))
    if records[-1]['reason'] != 'tail-kind': raise RuntimeError('Opaque reference was admitted without an exact Type witness')
    header = ['/* Generated only from the current compiler\'s typechecked source call plans. */',
              'typedef struct Native64GeneratedStore { unsigned int op, destination, fixed, ordinal; long source; unsigned int immediate; int cell; unsigned int source_offset; } Native64GeneratedStore;',
              'typedef struct Native64GeneratedSource { unsigned int bytes, mapbytes; const uchar *map; } Native64GeneratedSource;',
              'typedef struct Native64GeneratedCell { unsigned int space, offset, bytes, root, seed, length, high, low; const uchar *initial; unsigned int setupop, setuppc, setupmp, setupoffset, setupbytes; } Native64GeneratedCell;',
              'typedef struct Native64GeneratedBank { unsigned int bytes, count, mapbytes; const uchar *map; const Native64GeneratedSource *sources; const Native64GeneratedCell *cells; unsigned int steps; const unsigned int *order; } Native64GeneratedBank;']
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
        'utfseed': 'sys->sprint("%s|%s|%s", "", "Привет", "a\\0b");',
        'branchsetup': 'number := sys->millisec(); if(number < 0) number = -1; sys->sprint("%d", number);',
        'barriersetup': 'number := -1; ignored := sys->millisec(); sys->sprint("%d", number);',
        'overwritesetup': 'number := -1; number = -2; sys->sprint("%d", number);',
        'globalseed': 'sys->sprint("%d|%bd|%g", n, wide, fraction);',
        'typedglobal': 'sys->sprint("%d|%s|%bd|%g", seed.inner.count, seed.inner.text, seed.inner.wide, seed.fraction);',
        'typedlocal': 'seed: Outer; seed.lead=byte 123; seed.inner.count=-17; seed.inner.text="Grüße\\0世界"; seed.inner.wide=big -17; seed.fraction=-1.25; sys->sprint("%d|%s|%bd|%g", seed.inner.count, seed.inner.text, seed.inner.wide, seed.fraction);',
        'typedcopy': 'seed:=original; sys->sprint("%d|%s|%bd|%g", seed.inner.count, seed.inner.text, seed.inner.wide, seed.fraction);',
    }
    typed_interface='Probe: module { Inner: adt { count: int; text: string; wide: big; }; Outer: adt { lead: byte; inner: Inner; fraction: real; }; init: fn(); };'
    (work/'typedfield.m').write_text(typed_interface)
    (work/'typedfield-native64.h').write_text(checked_run([str(limbo),'-H',str(work/'typedfield.m')],env=env,check=True,capture_output=True,text=True).stdout)
    for label, body in extra_sources.items():
        probe = work/(label+'.b')
        interface='include "typedfield.m";' if label.startswith('typed') else 'Probe: module { init: fn(); };'
        globals=('n: int = -17; wide: big = big -17; fraction: real = -1.25; ' if label=='globalseed' else
                 ('seed' if label=='typedglobal' else 'original')+': Outer = Outer(byte 123, Inner(-17, "Grüße\\0世界", big -17), -1.25); ' if label in ('typedglobal','typedcopy') else '')
        probe.write_text('implement Probe; include "sys.m"; '+interface+globals+'init() { sys := load Sys Sys->PATH; '+body+' }')
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
            if label=='typedcopy':
                for fault in ('origin','descriptor','source','projection'):
                    altered=json.loads(json.dumps(extra_programs[label]))
                    cell=next(c for c in altered['bank']['cells'] if c['aggregate'] is not None)
                    if fault=='origin': cell['origin_offset']+=4
                    elif fault=='projection': cell['setup']['source_value']+=4
                    elif fault=='descriptor': cell['aggregate']['descriptor']=verify_locations(traced_probe.read_bytes(),probe_records,output.stderr)[probe_records[0]['id']]['caller_descriptor']
                    else: cell['aggregate']['source']+=4
                    changed='\n'.join('NATIVE64_CALL_LOWER '+json.dumps(altered) if line.startswith('NATIVE64_CALL_LOWER ') else line for line in output.stderr.splitlines())
                    try: verify_stores(traced_probe.read_bytes(),probe_records,changed)
                    except (RuntimeError,KeyError): pass
                    else: raise RuntimeError('Aggregate oracle accepted corrupt '+fault)
    for label in ('branchsetup','barriersetup'):
        if any(c['setup'] is not None for c in extra_programs[label]['bank']['cells'] if c['source_mode']=='fp'):
            raise RuntimeError('A branch or call silently became a local definition proof')
    local = next(c for c in extra_programs['overwritesetup']['bank']['cells'] if c['source_mode']=='fp')
    if local['setup']['input'] != dict(kind='word',high=0,low=0xfffffffe): raise RuntimeError('The latest FP definition was lost')
    if len(extra_programs['castalias']['bank']['cells']) != 2: raise RuntimeError('Big reads and conversions failed to share one typed source')
    if len(extra_programs['alias']['bank']['cells']) != 1: raise RuntimeError('Identical String sources lost their shared cell')
    for label, program in extra_programs.items(): header += emit_program('call'+label+'stores', program)
    for label in ('typedglobal','typedlocal','typedcopy'):
        program=extra_programs[label]
        bank=program['bank']; space=1 if label=='typedglobal' else 0
        byteleaves=[l for l in bank['sources'][space]['leaves'] if l[2:4]==[1,1]]
        if len(byteleaves)!=1: raise RuntimeError('Missing unique typed ADT byte prefix')
        oldbase,base=byteleaves[0][:2]
        fields=[s for s in program['stores'] if not s['fixed']]
        if [bank['cells'][s['source_cell']]['legacy_offset']-oldbase for s in fields]!=[8,12,16,24]:
            raise RuntimeError('Source ADT field locations changed')
        for store in fields:
            cell=bank['cells'][store['source_cell']]
            if cell['source_mode']!=('mp' if space else 'fp') or (cell['initial'] if space else (cell['setup'] or {}).get('input')) is None:
                raise RuntimeError('Nested field lost its typed source initializer/definition: '+str(cell))
        header += ['static const unsigned int call'+label+'base = '+str(base)+';']
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
