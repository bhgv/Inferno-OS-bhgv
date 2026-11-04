#!/usr/bin/env python3
"""Independent C-format integer and native font-description expectations."""
import json
import random
import re


# Recognize complete C decimal/octal/hex tokens, keeping overflow distinct from filenames.
def number(token):
    sign = -1 if token.startswith('-') else 1
    value = token[1:] if token.startswith(('-', '+')) else token
    if re.fullmatch(r'0[xX][0-9a-fA-F]+', value):
        base = 16
    elif re.fullmatch(r'0[0-7]*', value):
        base = 8
    elif re.fullmatch(r'[1-9][0-9]*', value):
        base = 10
    else:
        return 0, 0
    result = sign*int(value, base)
    return (1, result) if -2147483648 <= result <= 2147483647 else (-1, 0)


# Spell signed values mathematically without using a target libc conversion.
def spelling(value, base):
    prefix = '-' if value < 0 else ''
    value = abs(value)
    return prefix+(str(value) if base == 10 else '0'+format(value, 'o') if base == 8 else '0x'+format(value, 'x'))


# Emit static records with explicit expected fields; manifest/parser output is not an oracle.
def rows():
    rng = random.Random(117)
    tokens = ['', '0', '-0', '+0', '08', '0x', '0xG', '--1', '12file', '123x',
              '2147483647', '2147483648', '-2147483648', '-2147483649', '9'*80]
    tokens += [spelling(rng.randrange(-2**40, 2**40), rng.choice([8, 10, 16])) for _ in range(500)]
    output = ['''typedef struct FontNumberCase FontNumberCase;

struct FontNumberCase {
    char *token; // Complete input token.
    int status; // Filename0, Int32 one, complete overflow minus one.
    int value; // Expected Int32 value only on success.
};

static FontNumberCase numbers[] = {''']
    for token in tokens:
        status, value = number(token)
        output.append('{'+json.dumps(token)+','+str(status)+','+str(value)+'},')
    output.append('''};

typedef struct FontCase FontCase;

struct FontCase {
    char *text; // Complete description bytes.
    int valid; // Independent admission expectation.
    int height; // Expected height.
    int ascent; // Expected ascent.
    int low; // Inclusive Unicode minimum.
    int high; // Inclusive Unicode maximum.
    int offset; // Starting subfont position, preserving signed Int32 compatibility.
    char *name; // Exact retained filename.
};

static FontCase fontcases[] = {''')
    cases = []
    for index in range(300):
        height, ascent = rng.randrange(1, 32768), rng.randrange(1, 32768)
        low = rng.randrange(0, 0x110000)
        high = rng.randrange(low, 0x110000)
        offset = rng.choice([-2147483648, -1, 0, 2147483647, rng.randrange(-2**31, 2**31)])
        fields = [spelling(value, rng.choice([8, 10, 16])) for value in (height, ascent, low, high, offset)]
        name = 'part-'+str(index)+'.sub'
        cases.append((rng.choice([' ', '\t', '\r\n']).join(fields+[name]), 1, height, ascent, low, high, offset, name))
    cases += [('12 9 0x10ffff 0x10ffff glyph', 1, 12, 9, 0x10ffff, 0x10ffff, 0, 'glyph'),
              ('12 9 0 127 123 ', 1, 12, 9, 0, 127, 0, '123'),
              ('12 9 0 127 4294967296', 1, 12, 9, 0, 127, 0, '4294967296'),
              ('12 9 0 127 12file', 1, 12, 9, 0, 127, 0, '12file')]
    for text in ['12junk 9 0 1 a', '12 9junk 0 1 a', '08 9 0 1 a', '12 9 08 10 a',
                 '12 9 0 1junk a', '12 9 0 127 2147483648 glyph', '12 9 0 127 -2147483649 glyph',
                 '12 9 0 127 '+'9'*80+' glyph', '12 9 -1 127 a', '12 9 0 0x110000 a', '12 9 0 127 ', '12 9 0 127 first 128 255 2147483648 second',
                 '12 9 0 127 first 128junk 255 second', '12 9 0 127 first 128 0x110000 second']:
        cases.append((text, 0, 0, 0, 0, 0, 0, ''))
    for text, valid, height, ascent, low, high, offset, name in cases:
        output.append('{'+json.dumps(text)+','+','.join(map(str, [valid, height, ascent, low, high, offset]))+','+json.dumps(name)+'},')
    return '\n'.join(output)+'\n};\n'


# Decode committed bitmap descriptions independently of C token pointers and target integer conversion.
def corpus_rows(root):
    files, ranges = [], []
    for path in sorted((root/'fonts').rglob('*.font')):
        text = path.read_bytes().decode('utf-8')
        tokens = [value for value in re.split(r'[ \t\r\n\v\f]+', text) if value]
        height = ascent = 0
        decoded = []
        valid = len(tokens) >= 5
        try:
            status, height = number(tokens[0])
            valid &= status == 1 and 0 < height <= 32767
            status, ascent = number(tokens[1])
            valid &= status == 1 and 0 < ascent <= 32767
            position = 2
            while position < len(tokens):
                lo_status, low = number(tokens[position])
                hi_status, high = number(tokens[position+1])
                position += 2
                if lo_status != 1 or hi_status != 1 or not 0 <= low <= high <= 0x10ffff:
                    valid = False
                    break
                status, offset = number(tokens[position])
                if status != 0 and position+1 < len(tokens):
                    if status != 1:
                        valid = False
                        break
                    position += 1
                else:
                    offset = 0
                decoded.append((low, high, offset, tokens[position]))
                position += 1
            valid &= bool(decoded) and len(decoded) <= 32767
        except IndexError:
            valid = False
        start = len(ranges)
        if valid:
            ranges.extend(decoded)
        files.append((str(path.relative_to(root)), text, int(valid), height, ascent, start, len(decoded) if valid else 0))
    output = ['''typedef struct FontRangeCase FontRangeCase;

struct FontRangeCase {
    int low; // Expected inclusive range start.
    int high; // Expected inclusive range end.
    int offset; // Expected signed starting position.
    char *name; // Exact subfont filename bytes.
};

static FontRangeCase corpus_ranges[] = {''']
    for low, high, offset, name in ranges:
        output.append('{'+','.join(map(str, (low, high, offset)))+','+json.dumps(name, ensure_ascii=False)+'},')
    output.append('''};

typedef struct FontCorpusCase FontCorpusCase;

struct FontCorpusCase {
    char *path; // Committed description identity.
    char *text; // Exact description bytes, retaining CRLF.
    int valid; // Independent grammar decision.
    int height; // Expected inter-line height.
    int ascent; // Expected baseline ascent.
    int first; // Starting index in the independent range table.
    int count; // Exact expected number of ranges.
};

static FontCorpusCase corpus_fonts[] = {''')
    for path, text, valid, height, ascent, start, count in files:
        output.append('{'+json.dumps(path)+','+json.dumps(text, ensure_ascii=False)+','+
                      ','.join(map(str, (valid, height, ascent, start, count)))+'},')
    return '\n'.join(output)+'\n};\n'
