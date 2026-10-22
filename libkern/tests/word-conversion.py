#!/usr/bin/env python3
"""Check fixed 32-bit kernel conversions with Python integer expectations."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cases = []


# Record an exact result and consumed input length.
def add(text, base, signed, expected, end):
    cases.append((text, base, signed, expected, end))


# Render a magnitude using Python arbitrary-precision division.
def digits(value, base):
    alphabet = '0123456789abcdefghijklmnopqrstuvwxyz'
    result = ''
    while value:
        value, digit = divmod(value, base)
        result = alphabet[digit] + result
    return result or '0'


# Generate exact magnitudes independently of the production parsing algorithm.
for base in range(2, 37):
    values = {0, 1, base-1, base, 2**31, 2**32, 2**63, 2**128}
    for boundary in (2**31, 2**32):
        values.update(range(boundary-40, boundary+41))
    for value in sorted(values):
        for sign in ('', '+', '-'):
            text = ' \t\n\r\f\v'+sign+digits(value, base)+'!'
            number = -value if sign == '-' else value
            add(text, base, True, max(-2**31, min(2**31-1, number)), len(text)-1)
            unsigned = 2**32-1 if value >= 2**32 else number % 2**32
            add(text, base, False, unsigned, len(text)-1)

for signed in (False, True):
    for text in ('', ' ', '-', '+', ' \t-', 'xyz'):
        add(text, 10, signed, 0, 0)
    for base in (-100, -1, 1, 37, 100):
        for text in ('0', '-101', ' +123'):
            add(text, base, signed, 0, 0)
    for text, base, value, end in (
        ('0', 0, 0, 1), ('077!', 0, 63, 3), ('09', 0, 0, 1),
        ('0x123!', 0, 291, 5), ('-0Xff!', 16, -255, 5),
        ('+0x80000000!', 0, 2**31, 11),
        ('0xffffffff!', 16, 2**32-1, 10),
        ('-0x100000000!', 0, -2**32, 12),
    ):
        expected = max(-2**31, min(2**31-1, value)) if signed else (
            2**32-1 if abs(value) >= 2**32 else value % 2**32)
        add(text, base, signed, expected, end)
    # Preserve the existing signed converter's no-digit hexadecimal-prefix rule.
    for text in ('0x', '0x!', '0Xg'):
        for base in (0, 16):
            add(text, base, signed, 0, 0 if signed else 1)
    for sign in ('', '-'):
        text = sign+'9'*4096+'!'
        add(text, 10, signed, (-2**31 if sign else 2**31-1) if signed else 2**32-1, len(text)-1)

# atol has a historical base-zero grammar and permits spaces after the sign.
for base, prefix in ((8, '0'), (10, ''), (16, '0x'), (16, '0X')):
    for value in list(range(100)) + [2**bit+delta for bit in range(30, 130) for delta in (-1, 0, 1)]:
        for sign in ('', '+', '-'):
            text = ' \t'+sign+' \t'+prefix+digits(value, base)+'!'
            number = (-value if sign == '-' else value) % 2**32
            if number >= 2**31:
                number -= 2**32
            add(text, 0, 2, number, 0)

with tempfile.TemporaryDirectory(prefix='kernel-word-') as directory:
    td = Path(directory)
    bodies = []
    for name in ('strtol', 'strtoul', 'atol'):
        path = Path(os.environ.get('KERNEL_'+name.upper()+'_SOURCE', root / 'libkern' / (name+'.c')))
        body = re.sub(r'#include[^\n]*', '', path.read_text())
        body = re.sub(r'\b(strtol|strtoul|atol|atoi|LONG_MAX|LONG_MIN|ULONG_MAX)\b', r'kernel_\1', body)
        bodies.append(body)
    rows = ',\n'.join('{%s,%d,%d,%dLL,%d}' % (json.dumps(t).replace(chr(92)+'u000b', chr(92)+'v'), b, s, n, e)
                       for t, b, s, n, e in cases)
    for scalar in ('unsigned int', 'unsigned long'):
        source = '''#include <stdio.h>
#include <stddef.h>
typedef SCALAR ulong;
/* Disable only unavailable LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }
BODIES
struct Case { char *text; int base, sign; long long value; ptrdiff_t end; };
static struct Case cases[] = { ROWS };
/* Check values, consumed input, and the optional end-pointer path. */
int main(void) {
    size_t i; char *end; long long value, again;
    for(i=0; i<sizeof(cases)/sizeof(cases[0]); i++) {
        struct Case *c = &cases[i];
        if(c->sign == 2) {
            value = kernel_atol(c->text);
            again = kernel_atoi(c->text);
            end = c->text;
        } else if(c->sign) {
            value = kernel_strtol(c->text, &end, c->base);
            again = kernel_strtol(c->text, NULL, c->base);
        } else {
            value = kernel_strtoul(c->text, &end, c->base);
            again = kernel_strtoul(c->text, NULL, c->base);
        }
        if(value != c->value || again != value || end-c->text != c->end) {
            fprintf(stderr, "case %zu base %d signed %d: value %lld expected %lld end %td expected %td\\n",
                i, c->base, c->sign, value, c->value, end-c->text, c->end);
            return 1;
        }
    }
    printf("kernel word conversions: %zu cases, ulong=%zu bytes\\n",
        sizeof(cases)/sizeof(cases[0]), sizeof(ulong));
    return 0;
}
'''.replace('SCALAR', scalar).replace('BODIES', '\n'.join(bodies)).replace('ROWS', rows)
        unit = td / 'test.c'
        unit.write_text(source)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+[str(unit), '-o', str(td/'test')], check=True)
        subprocess.run([str(td/'test')], check=True)
