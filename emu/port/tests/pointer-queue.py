#!/usr/bin/env python3
"""Check queued button transitions, motion coalescing and overflow recovery."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1].joinpath('devpointer.c').read_text()
mouse = source[source.index('typedef struct Pointer'):source.index('static\nDirtab')]
queue_start = source.index('enum {\n    Nevent')
queue = source[queue_start:source.index('#if 0', queue_start)]
driver = source[source.index('void\nmousetrack'):source.index('static Chan*\npointerattach')]

header = r'''
#include <assert.h>
typedef unsigned int uint;
typedef unsigned long ulong;
typedef struct { int held; } Lock;
typedef int Rendez;
typedef int Ref;
typedef int QLock;
typedef struct { int x, y; } Point;
#define USED(x) (void)(x)
static Point Pt(int x, int y) { return (Point){x, y}; }
static ulong osmillisec(void) { static ulong ticks; return ++ticks; }
static void lock(Lock *l) { assert(!l->held); l->held = 1; }
static void unlock(Lock *l) { assert(l->held); l->held = 0; }
'''

transport = r'''
static void Sleep(Rendez *r, int (*ready)(void *), void *arg)
{
    USED(r);
    assert(ready(arg));
}
static void Wakeup(Rendez *r) { USED(r); assert(!ptrq.lock.held); }
'''

test = r'''
static void
expect(int buttons, int x, int y)
{
    Pointer p = mouseconsume();
    assert(p.b == buttons && p.x == x && p.y == y);
}

int
main(void)
{
    Point p;
    int i;
    mousetrack(1, 4, 8, 0);
    mousetrack(0, 4, 8, 0);
    expect(1, 4, 8);
    expect(0, 4, 8);
    assert(!ptrqnotempty(0));

    mousetrack(1, 5, 6, 0);
    mousetrack(1, 2, 3, 1);
    mousetrack(1, 2, 3, 1);
    expect(1, 5, 6);
    expect(1, 9, 12);
    assert(!ptrqnotempty(0));

    for(i = 0; i < Nevent; i++)
        mousetrack(i % 2, i, 0, 0);
    assert(ptrq.full);
    mousetrack(0, 99, 100, 0);
    for(i = 0; i < Nevent; i++)
        expect(i % 2, i, 0);
    expect(0, 99, 100);
    assert(!ptrqnotempty(0));
    p = mousexy();
    assert(p.x == 99 && p.y == 100);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='pointer-queue-') as directory:
    unit = Path(directory) / 'pointer.c'
    binary = unit.with_suffix('')
    unit.write_text(header + mouse + queue + transport + driver + test)
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run(compiler + ['-std=c99', '-Wall', '-Wextra', '-Werror', str(unit),
                               '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS pointer queue: buffered press/release, latest motion and full-ring recovery')
