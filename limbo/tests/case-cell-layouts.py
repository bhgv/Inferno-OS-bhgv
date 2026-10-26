#!/usr/bin/env python3
"""Check production Limbo case serializers against declared cell-width offsets."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT/'limbo/dis.c').read_text()
source = source[source.index('static void\ndisdatum('):]
arms = []
for kind in ('Tcase', 'Tcasel', 'Tgoto'):
    match = re.search(r'\tcase '+kind+r':[\s\S]*?\t\tbreak;', source)
    if match is None:
        raise RuntimeError('missing serializer '+kind)
    arms.append(match.group())
com = (ROOT/'limbo/com.c').read_text()
start = com.index('Long\ncaseend(')
end = com.index('\n}', start)+2
helper = com[start:end]
unit = r'''
#include "limbo.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct Event Event;
struct Event {
    long offset; // Compiler-selected module-data byte offset.
    Long value; // Serialized signed value before wire compression.
    int width; // Meaningful value width, independent of padding cells.
};
static Event events[64];
static int used, readat;


/* Record the compiler's actual data writes without replacing offset arithmetic. */
static void
record(long offset, Long value, int width)
{
    if(used >= nelem(events))
        exit(1);
    events[used].offset = offset;
    events[used].value = value;
    events[used++].width = width;
}


/* Capture the real serializer's integer records. */
static void
disint(long offset, Long value)
{
    record(offset, value, 4);
}


/* Capture the real serializer's big records. */
static void
disbig(long offset, Long value)
{
    record(offset, value, 8);
}


/* Keep inclusive-to-exclusive bounds in the compiler's actual helper. */
HELPER


/* Execute extracted production switch arms with real compiler data structures. */
static void
emitcasecells(Node *n, long offset, int kind)
{
    Case *c;
    Label *lab;
    int i;

    switch(kind){
ARMS
    }
}


/* Compare the event stream with an independent byte-offset oracle. */
static void
expect(long offset, Long value, int width)
{
    Event *event;

    if(readat >= used)
        exit(2);
    event = &events[readat++];
    if(event->offset != offset || event->value != value || event->width != width){
        fprintf(stderr, "case serializer mismatch cell=%d event=%d offset=%ld expected=%ld\n",
            IBY2WD, readat-1, event->offset, offset);
        exit(3);
    }
}


/* Exercise each table family with and without a wildcard, keeping pointer and word sizes separate. */
int
main(void)
{
    int kinds[] = {Tcase, Tcasel, Tgoto};
    Node container, bounds[10];
    Type type;
    Case c;
    Label labels[5];
    Inst targets[6];
    int family, count, wildcard, row, base, header, stride, checks;

    memset(&container, 0, sizeof(container));
    memset(&type, 0, sizeof(type));
    memset(&c, 0, sizeof(c));
    memset(labels, 0, sizeof(labels));
    memset(bounds, 0, sizeof(bounds));
    memset(targets, 0, sizeof(targets));
    container.ty = &type;
    type.cse = &c;
    c.labs = labels;
    for(row = 0; row < 5; row++){
        labels[row].start = &bounds[2*row];
        labels[row].stop = &bounds[2*row+1];
        bounds[2*row].val = row*10;
        bounds[2*row+1].val = row*10+3;
        targets[row].pc = row+1;
        labels[row].inst = &targets[row];
    }
    targets[5].pc = 31;
    checks = 0;
    base = 64;
    for(family = 0; family < 3; family++)
    for(count = 0; count <= 5; count++)
    for(wildcard = 0; wildcard < 2; wildcard++){
        c.nlab = count;
        c.iwild = wildcard ? &targets[5] : nil;
        type.size = (count+1+wildcard)*IBY2WD;
        used = readat = 0;
        emitcasecells(&container, base, kinds[family]);
        expect(base, family == 2 ? count+wildcard : count, 4);
        if(family < 2){
            header = family == 1 ? 2*IBY2WD : IBY2WD;
            stride = family == 1 ? 16+2*IBY2WD : 3*IBY2WD;
            for(row = 0; row < count; row++){
                expect(base+header+row*stride, row*10, family == 1 ? 8 : 4);
                expect(base+header+row*stride+(family == 1 ? 8 : IBY2WD), row*10+4, family == 1 ? 8 : 4);
                expect(base+header+row*stride+(family == 1 ? 16 : 2*IBY2WD), row+1, 4);
            }
            expect(base+header+count*stride, wildcard ? 31 : -1, 4);
        }else{
            for(row = 0; row < count; row++)
                expect(base+(row+1)*IBY2WD, row+1, 4);
            if(wildcard)
                expect(base+(count+1)*IBY2WD, 31, 4);
        }
        if(readat != used)
            return 4;
        checks++;
    }
    printf("PASS compiler case cells: %d production layouts, cell=%d\n", checks, IBY2WD);
    return 0;
}
'''.replace('ARMS', '\n'.join(arms)).replace('HELPER', helper)
with tempfile.TemporaryDirectory(prefix='case-cell-layouts-') as directory:
    td = Path(directory)
    target = td/'cells.c'
    target.write_text(unit)
    for cells in (4, 8):
        model = td/str(cells)
        model.mkdir()
        isa = re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(cells), (ROOT/'include/isa.h').read_text())
        (model/'isa.h').write_text(isa)
        # limbo.h includes isa.h from the include search path, so the model owns that constant.
        binary = model/'cells'
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        includes = [model]+[ROOT/path for path in ('Linux/arm/include', 'limbo', 'include')]
        subprocess.run(command+['-I'+str(path) for path in includes]+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
