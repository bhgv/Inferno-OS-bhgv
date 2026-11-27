#!/usr/bin/env python3
"""Independent bounded classic array initializer oracle and refusal witnesses."""
from pathlib import Path
import runpy
import struct

ROOT = Path(__file__).resolve().parents[2]
read = runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))['legacy_instructions']


def number(value):
    if -64 <= value < 64: return bytes([value & 127])
    if -8192 <= value < 8192: return ((value & 16383) | 32768).to_bytes(2, 'big')
    return ((value & 0x3fffffff) | 0xc0000000).to_bytes(4, 'big')


def image(data):
    # Eight-byte MP, scalar Word element, String element. Code is irrelevant.
    header = b''.join(number(n) for n in (819248,0,64,0,8,3,0,-1,-1))
    descriptors = b'\x00\x08\x01\x80\x01\x04\x00\x02\x04\x01\x80'
    return header+descriptors+data


def array(descriptor=1, length=3, offset=0):
    return b'\x51'+number(offset)+struct.pack('>II',descriptor,length)


def descend(offset=0, index=0):
    return b'\x61'+number(offset)+struct.pack('>I',index)


def word(offset=0, value=7):
    return b'\x21'+number(offset)+struct.pack('>I',value)


def string(offset=0, value=b'hi'):
    return bytes([0x30 | len(value)])+number(offset)+value


def main():
    pop=b'\x71\x00'; end=b'\x00'
    numeric=array()+descend(index=1)+word()+pop+end
    data=read(image(numeric),with_types=True,with_data=True,with_arrays=True)[3]
    assert data[0]['initial'] == {4:dict(kind='word',high=0,low=7)}
    strings=array(2,2)+descend()+string()+string(4,b'\xe2\x98\x83')+pop+string(4,b'outside')+end
    data=read(image(strings),with_types=True,with_data=True,with_arrays=True)[3]
    assert bytes(data[0]['initial'][4]['bytes']).decode() == '\u2603'
    assert bytes(data[4]['bytes']) == b'outside'
    # Separate descents keep offsets relative to their selected element and
    # retain earlier writes, so re-entering cannot hide an overlapping record.
    reenter=array()+descend()+word()+pop+descend(index=2)+word(value=9)+pop+end
    data=read(image(reenter),with_types=True,with_data=True,with_arrays=True)[3]
    assert sorted(data[0]['initial']) == [0,8]
    read(image(array(length=0)+end),with_types=True,with_data=True,with_arrays=True)
    bad={
        'missing-pop':array()+descend()+word()+end,
        'bare-pop':pop+end,
        'pop-offset':array()+descend()+b'\x71\x01'+end,
        'unknown-descriptor':array(7)+end,
        'length-limit':array(length=65537)+end,
        'negative-length':array(length=0xffffffff)+end,
        'negative-offset':array(offset=-1)+end,
        'root-outside':array(offset=8)+end,
        'index-outside':array()+descend(index=4)+end,
        'index-negative':array()+descend(index=0xffffffff)+end,
        'descent-not-array':word()+descend()+end,
        'empty-array-write':array(length=0)+descend()+word()+pop+end,
        'element-crossing':array()+descend(index=3)+word()+pop+end,
        'overlap':array()+descend()+word()+word()+pop+end,
        'overlap-reentry':array()+descend()+word()+pop+descend()+word()+pop+end,
        'root-overlap':array()+string()+end,
        'bad-control-count':b'\x52\x00'+struct.pack('>II',1,3)+end,
        'truncated-array':array()[:-1],
        'truncated-index':array()+descend()[:-1],
        'truncated-integer':b'\x51\xc0',
        'truncated-scalar':array()+descend()+word()[:-1],
        'truncated-string':array(2)+descend()+string()[:-1],
        'missing-terminator':array(),
        'depth-limit':(array(0,1)+descend())*5+end,
    }
    for label, payload in bad.items():
        try: read(image(payload),with_types=True,with_data=True,with_arrays=True)
        except RuntimeError: pass
        else: raise RuntimeError('initializer refusal lost: '+label)
    try: read(image(numeric),with_types=True,with_data=True,with_arrays=False)
    except RuntimeError as error: assert str(error)=='Non-flat classic initializer in this bounded source gate'
    else: raise RuntimeError('flat-only reader admission changed')
    print(f'PASS classic array oracle: 4 positive graphs, {len(bad)} malformed refusals; explicit flat-only policy preserved')


if __name__=='__main__': main()
