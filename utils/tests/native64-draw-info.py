#!/usr/bin/env python3
"""Independent bounded-integer and fixed-field record oracles for the actual native Draw parser."""
import json
import random
import re


# Python integers keep the protocol range independent of the target's C integer widths.
def integer(field):
    if not re.fullmatch(r' *[+-]?[0-9]+ *', field):
        return None
    value = int(field)
    return value if -2147483648 <= value <= 2147483647 else None


# Emit deterministic valid/malformed fields and complete records with independently derived results.
def rows():
    rng = random.Random(114)
    fields = [' '*12, 'x'.rjust(12), '2147483648'.rjust(12), '-2147483649'.rjust(12),
              '999999999999', '-2147483648 ', '+2147483647 ', '1 2'.ljust(12), '1\x00'.ljust(12)]
    fields += [str(rng.randrange(-10**11, 10**11)).rjust(12) for _ in range(500)]
    fields += [str(x).rjust(12) for x in (-2147483648, 2147483647, -1, 0, 1)]
    output = ['''typedef struct InfoIntegerCase InfoIntegerCase;

struct InfoIntegerCase {
    char field[12]; // Exact fixed-width bytes, including malformed NULs.
    int valid; // Independently evaluated Int32 admission.
    int value; // Expected value only when admitted.
};

static InfoIntegerCase integercases[] = {''']
    for field in fields:
        value = integer(field)
        output.append('{'+json.dumps(field).replace('\\u0000', '\\000')+','+str(int(value is not None))+','+str(value if value is not None else 0)+'},')
    output.append('''};

typedef struct InfoRecordCase InfoRecordCase;

struct InfoRecordCase {
    char record[144]; // Complete native device record.
    int valid; // Independent range/shape decision.
    int values[12]; // Expected scalar fields for valid records.
};

static InfoRecordCase recordcases[] = {''')
    for index in range(400):
        values = [7, 8, 0, 1, -20, -30, 50, 60, -200, -300, 500, 600]
        record = [str(value).rjust(11)+' ' for value in values]
        record[2] = 'r8g8b8a8'.rjust(11)+' '
        if index:
            field = rng.choice([0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11])
            record[field] = rng.choice(fields)
        decoded = [integer(field) if pos != 2 else 0 for pos, field in enumerate(record)]
        valid = (all(value is not None for value in decoded) and decoded[0] >= 0 and decoded[1] >= 0
                 and decoded[3] in (0, 1) and decoded[4] <= decoded[6] and decoded[5] <= decoded[7]
                 and decoded[8] <= decoded[10] and decoded[9] <= decoded[11])
        output.append('{'+json.dumps(''.join(record)).replace('\\u0000', '\\000')+','+str(int(valid))+',{'+','.join(str(x if valid else 0) for x in decoded)+'}},')
    output.append('};')
    return '\n'.join(output)+'\n'
