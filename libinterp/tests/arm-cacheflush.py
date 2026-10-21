#!/usr/bin/env python3
"""Check production Linux/Android segflush arguments using a syscall stub."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
for platform in ('Linux', 'Android'):
    source = (root / 'emu' / platform / 'segflush-arm.c').read_text()
    source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
    unit = r'''
#include <assert.h>
#include <stddef.h>
typedef unsigned long ulong;
#define __ARM_NR_cacheflush 0xf0002
static int calls, result;
static char *expected_start, *expected_end;
static long stub_syscall(long number, void *start, void *end, int flags) {
    assert(number == __ARM_NR_cacheflush);
    assert(start == expected_start);
    assert(end == expected_end);
    assert(flags == 0);
    calls++;
    return result;
}
#define syscall stub_syscall
''' + source + r'''
int main(void) {
    char code[64];
    expected_start = code;
    assert(segflush(code, 0) == 0);
    assert(calls == 0);
    expected_end = code+1;
    assert(segflush(code, 1) == 0);
    assert(calls == 1);
    expected_end = code+sizeof(code);
    assert(segflush(code, sizeof(code)) == 0);
    assert(calls == 2);
    result = -1;
    assert(segflush(code, sizeof(code)) == -1);
    assert(calls == 3);
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='arm-cacheflush-') as td:
        td = Path(td)
        (td / 'probe.c').write_text(unit)
        subprocess.run(shlex.split(os.environ.get('CC', 'cc')) +
                       shlex.split(os.environ.get('CFLAGS', '-O1 -g')) +
                       [str(td / 'probe.c'), '-o', str(td / 'probe')], check=True)
        subprocess.run([str(td / 'probe')], check=True)
print('PASS Linux/Android ARM cacheflush: empty, one-byte, full range and error propagation')
