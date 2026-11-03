#!/usr/bin/env python3
"""Exercise actual unpublished load64 scalar-call construction, rollback and source-independent C/RX execution."""
import hashlib
import json
from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parents[2]
INFER = runpy.run_path(str(ROOT/'utils/tests/dis64-call-infer.py'))
EXTRACT = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']


# Replace the trusted fixture constructor with the actual owning loader implementation.
def unit(control):
    source = INFER['unit'](None)
    old = EXTRACT(source, 'static Module*\nresultmodule(')
    helper = (ROOT/'utils/tests/dis64-call-construct.c').read_text()
    source = source.replace(old, helper, 1)
    marker = '\t\tresultcheck(status == valid && allocations == before && live == 0, "whole-image decision mismatch");'
    if source.count(marker) != 1: raise RuntimeError('Constructor file-boundary drift')
    setup = '''
        ctorinput = imagebytes;
        ctorbytes = bytes;
        memmove(ctorsaved, imagebytes, bytes);
        if(status){
            resultentry = image.module.entry;
            resultframe = image.module.frametype;
            resultcount = image.instructions;
            resulttypes = image.types;
        }
        ctorprobe(valid);
'''
    source = source.replace(marker, marker+setup, 1)
    # Source gets poisoned only during the subsequent execution matrix, after these probes.
    source = source.replace('PASS inferred-image calls:', 'PASS actual-constructor calls:', 1)
    summary = '\tprintf("PASS actual constructor ownership: %d accepted/%d refused C-RX constructions, %d injected allocation refusals; source poisoned before execution\\n", ctoraccepted, ctorrefused, ctorfaults);\n'
    pos = source.rfind('\n\treturn 0;\n}')
    if pos == -1: raise RuntimeError('Constructor main-boundary drift')
    source = source[:pos]+'\n'+summary+source[pos:]
    if control == 'workspace':
        old = '\tfree(load.work);\n\tif(!success)'
        new = '\tif(0) free(load.work);\n\tif(!success)'
    elif control == 'partial':
        old = 'if(!success && module != nil)'
        new = 'if(0 && !success && module != nil)'
    elif control == 'mode':
        old = 'module->cstep = native64lea8step;'
        new = 'module->cstep = nil;'
    elif control == 'lowering':
        old = 'instruction->d.i.s = decoded->secondary;'
        new = 'instruction->d.i.s = 0;'
    elif control is not None:
        raise RuntimeError('Unsupported constructor control')
    if control is not None:
        signature = 'Module*\nprepare64calls(' if control in ('workspace', 'partial') else 'static int\ndis64callconstruct('
        actual = EXTRACT(source, signature)
        if actual.count(old) != 1: raise RuntimeError('Constructor source control drift: '+control)
        source = source.replace(actual, actual.replace(old, new, 1), 1)
    return source


# Keep a canonical frame-ordinal permutation distinct from the original39 peer inputs.
def reordered_case(write=False):
    calls = INFER['CALLS']
    root = [calls.INIT.move(42, 64), calls.frame(typeid=0), calls.indirect('IMOVW', 64, 40, 64),
            calls.indirect('ILEA', 104, 40, 32), calls.call(7), calls.INIT.move(104, 80, 1, 'W'), calls.EXIT]
    case = calls.fixture(root, leaves=[[calls.RETURNS.store(1, 64, 1), calls.RET]], slots=[(), ()],
                         signatures=[(), (1,)], results=[0, 1], frames=[1, 0], sizes=[88, 112])
    image = INFER['BASE']['container'](case, calls.words(0, 1, 0, 0, 1))
    directory = ROOT/'utils/tests/goldens/dis64-construct'
    content = '\n'.join(image.hex()[i:i+64] for i in range(0, len(image)*2, 64))+'\n'
    manifest = json.dumps({'name': 'reordered-types42', 'bytes': len(image),
                          'sha256': hashlib.sha256(image).hexdigest(), 'root_frame': 1,
                          'leaf_frame': 0, 'result': 42, 'public_accept': False}, indent=2)+'\n'
    if write:
        directory.mkdir(parents=True, exist_ok=True)
        (directory/'reordered-types42.hex').write_text(content)
        (directory/'manifest.json').write_text(manifest)
    elif (directory/'reordered-types42.hex').read_text() != content or (directory/'manifest.json').read_text() != manifest:
        raise RuntimeError('Constructor frame-order golden drift')
    return [('reordered-types42', image, True, 1, 42, 0, 0, 2)]


# Reuse39 immutable inputs plus a Type permutation; actual construction and cleanup remain owning source.
def main():
    import sys
    if '--write-goldens' in sys.argv:
        reordered_case(write=True)
        print('WROTE canonical constructor frame-order image')
        return
    INFER['main'](unit_factory=unit, controls=(None, 'workspace', 'partial', 'mode', 'lowering'),
                  label='actual constructor', extra_rows=reordered_case())


if __name__ == '__main__':
    main()
