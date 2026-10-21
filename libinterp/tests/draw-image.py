#!/usr/bin/env python3
"""Run production Draw image loading with fault injection and real PNG/SVG codecs."""
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import zlib
root=Path(__file__).resolve().parents[2]
source=(root/'libinterp/draw.c').read_text()
implementation=source[source.index('/* loadimage/nsvg strides'):source.index('void\nDisplay_open(')]
harness=(root/'libinterp/tests/draw-image.c').read_text().replace('DRAWIMAGE_IMPLEMENTATION',implementation)
def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
with tempfile.TemporaryDirectory(prefix='draw-image-') as directory:
    td=Path(directory)
    fixtures=td/('a'*70)/('b'*70);fixtures.mkdir(parents=True)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,1,8,6,0,0,0))
    png+=chunk(b'IDAT',zlib.compress(b'\0'+bytes([200,100,50,128,255,0,0,255])))+chunk(b'IEND',b'')
    (fixtures/'sample.PnG').write_bytes(png)
    (fixtures/'sample.SvG').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="2" height="1"><rect width="2" height="1" fill="red"/></svg>')
    (fixtures/'alpha.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="2" height="1"><rect width="2" height="1" fill="red" fill-opacity="0.5"/></svg>')
    (fixtures/'gradient.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="4" height="1"><defs><linearGradient id="g"><stop stop-color="red" offset="0"/><stop stop-color="blue" offset="1"/></linearGradient></defs><rect width="4" height="1" fill="url(#g)"/></svg>')
    (fixtures/'broken.png').write_bytes(b'bad png')
    (fixtures/'broken.svg').write_text('bad svg')
    unit=td/'draw-image.c';unit.write_text(harness)
    common=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g'))
    common+=['-Dmalloc=testmalloc','-Drealloc=testrealloc','-Dfree=testfree']
    common+=['-I'+str(root/p) for p in ['Linux/386/include','include']]
    for real in [False,True]:
        target=td/('real' if real else 'faults')
        command=common+(['-DREAL_DECODERS'] if real else [])+[str(unit),str(root/'lib9/cistrcmp.c')]
        if real:command += [str(root/'liblodepng/lodepng.c'),str(root/'lib9/qsort.c')]
        command+=['-lm','-o',str(target)]
        subprocess.run(command,check=True)
        environment=dict(os.environ)
        environment.setdefault('UBSAN_OPTIONS','halt_on_error=1')
        subprocess.run([str(target),str(fixtures)],check=True,env=environment)
