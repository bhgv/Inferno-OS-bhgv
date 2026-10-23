#!/usr/bin/env python3
"""Check both production Dis operand writers against the signed wire format."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('DIS_CONSTANT_SOURCE', root/'limbo/dis.c')).read_text()
parts = []
for name in ('discon', 'disbcon'):
    start = source.index('void\n'+name+'(long val)')
    parts.append(source[start:source.index('\n}', start)+2])
unit = r'''
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static jmp_buf escape;
static unsigned char streamed[16], ibuf[16];
static int bout, nstreamed, nibuf, errors, checks;
/* Keep writes observable and bounded without replacing the operand encoding. */
static void Bputc(int stream, int value)
{
    (void)stream;
    assert(nstreamed < 16);
    streamed[nstreamed++] = value;
}
/* A rejected operand must fail before either writer publishes any byte. */
static void fatal(const char *message, ...)
{
    assert(message != NULL);
    errors++;
    longjmp(escape, 1);
}
/* Disable only unsupported LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }
IMPLEMENTATION
/* Independently construct the wire bytes using unsigned masking. */
static void check(long value)
{
    unsigned char expected[4];
    uint32_t bits;
    int valid, length, i;

    valid = value >= -536870912L && value <= 536870911L;
    length = value >= -64 && value <= 63 ? 1 :
        value >= -8192 && value <= 8191 ? 2 : 4;
    bits = (uint32_t)value;
    for(i = 0; i < length; i++)
        expected[i] = bits >> (8*(length-i-1));
    expected[0] = (expected[0] & (length == 1 ? 127 : 63)) |
        (length == 1 ? 0 : length == 2 ? 128 : 192);
    memset(streamed, 0xa5, sizeof(streamed));
    memset(ibuf, 0xa5, sizeof(ibuf));
    nstreamed = nibuf = errors = 0;
    if(setjmp(escape) == 0)
        discon(value);
    assert(errors == !valid && nstreamed == (valid ? length : 0));
    if(valid)
        assert(memcmp(streamed, expected, length) == 0);
    errors = 0;
    if(setjmp(escape) == 0)
        disbcon(value);
    assert(errors == !valid && nibuf == (valid ? length : 0));
    if(valid)
        assert(memcmp(ibuf, expected, length) == 0);
    for(i = valid ? length : 0; i < 16; i++)
        assert(streamed[i] == 0xa5 && ibuf[i] == 0xa5);
    checks += 2;
}
/* Include all encoding transitions and LP64 negatives that used to truncate. */
int main(void)
{
    long boundaries[] = {LONG_MIN, LONG_MIN+1, -536870913L, -536870912L,
        -536870911L, -8193, -8192, -8191, -65, -64, -63, -1, 0, 1,
        62, 63, 64, 8190, 8191, 8192, 536870910L, 536870911L,
        536870912L, LONG_MAX};
    uint32_t random = 0x6abc1234;
    long value;
    int i;

    for(i = 0; i < (int)(sizeof(boundaries)/sizeof(boundaries[0])); i++)
        check(boundaries[i]);
    if(LONG_MAX > 2147483647L){
        check(-4294967297LL);
        check(-8589934593LL);
        check(-4294967296LL-536870912LL);
        check(-4294967296LL+536870911LL);
    }
    for(i = 0; i < 20000; i++){
        random = random*1664525U+1013904223U;
        value = random & 0x3fffffff;
        if(value >= 0x20000000)
            value -= 0x40000000;
        check(value);
    }
    printf("Dis constants: %d writer cases\n", checks);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='dis-constants-') as directory:
    td = Path(directory)
    target = td/'test.c'
    target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
    binary = td/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
