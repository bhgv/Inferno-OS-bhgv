#!/usr/bin/env python3
"""Check actual divw/divl/modw/modl against Python integers under strict UBSan."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile


# Extract an unchanged complete owning handler, skipping braces in comments and
# literals. source is borrowed text; name is an exact existing OP identifier.
def function(source, name):
    start = source.index("OP(" + name + ")")
    brace = source.index("{", start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:brace + token.end()]
    raise RuntimeError("Unterminated owning handler: " + name)


# Emit exact int64 C literals, including MIN without an overflowing unary minus.
def literal(value):
    if value == -(1 << 63):
        return "(-INT64_C(9223372036854775807)-1)"
    return ("-" if value < 0 else "") + "INT64_C(" + str(abs(value)) + ")"


# Derive expectations with arbitrary precision and magnitude truncation. Seeded
# full-width values and every positive/negative power divisor cover signed edges;
# MIN/-1 explicitly wraps, and remainder keeps the dividend sign. No C transfer
# formula or production branch is reused as the arithmetic oracle.
def vectors(width, name):
    minimum, maximum = -(1 << (width-1)), (1 << (width-1))-1
    divisors = {minimum, -3, 3}
    for shift in range(width-1):
        divisors.update((1 << shift, -(1 << shift)))
    rows = []
    state = 0x9e3779b97f4a7c15
    for divisor in sorted(divisors):
        values = {minimum, minimum+1, maximum, maximum-1, -257, -256, -255,
                  -9, -8, -7, -1, 0, 1, 7, 8, 9, 255, 256, 257}
        for edge in (divisor-1, divisor, divisor+1, -divisor-1, -divisor, -divisor+1):
            if minimum <= edge <= maximum:
                values.add(edge)
        for unused in range(24):
            state = (state * 6364136223846793005 + 1442695040888963407) & ((1 << 64)-1)
            value = state & ((1 << width)-1)
            values.add(value if value <= maximum else value-(1 << width))
        for dividend in sorted(values):
            quotient = abs(dividend) // abs(divisor)
            if (dividend < 0) != (divisor < 0):
                quotient = -quotient
            remainder = dividend - quotient * divisor
            if quotient > maximum:
                quotient = minimum
            rows.append("{" + ",".join(map(literal, (dividend, divisor, quotient, remainder))) + "}")
    return "static const int64_t " + name + "[][4] = {\n" + ",\n".join(rows) + "\n};\n"


# Only native arithmetic/error transport is bounded. Compile unchanged owning
# handlers for both WORD models; generated arrays and binaries stay in private
# temporary scratch. No decoder, scheduler, full VM or target execution is claimed.
def main():
    root = Path(__file__).resolve().parents[2]
    source = (root / "libinterp/xec.c").read_text()
    template = (root / "libinterp/tests/division-powers.c").read_text()
    implementation = "\n".join(function(source, name) for name in ("divw", "divl", "modw", "modl"))
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", "-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all"))
    with tempfile.TemporaryDirectory(prefix="division-powers-") as directory:
        work = Path(directory)
        for width in (32, 64):
            unit = work / ("division" + str(width) + ".c")
            unit.write_text(template.replace("IMPLEMENTATION", implementation).replace(
                "VECTORS", vectors(width, "word_vectors") + vectors(64, "big_vectors")))
            binary = work / ("division" + str(width))
            subprocess.run(compiler + flags + ["-DWORD_BITS=" + str(width), str(unit), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            # Break only the MIN/-1 wrap result; the independent byte oracle must
            # fail, independently of whether UBSan happens to trap an overflow.
            body = unit.read_text()
            if body.count("result = dividend;") != 2:
                raise RuntimeError("Owning MIN-division control boundary changed")
            control = work / ("division-control" + str(width) + ".c")
            control.write_text(body.replace("result = dividend;", "result = 0;", 1))
            target = work / ("division-control" + str(width))
            subprocess.run(compiler + flags + ["-DWORD_BITS=" + str(width), str(control), "-o", str(target)], check=True)
            rejected = subprocess.run([str(target)], capture_output=True, text=True)
            if rejected.returncode == 0 or "Division byte mismatch" not in rejected.stderr:
                raise RuntimeError("MIN-division control did not reach the active byte oracle")
            print("PASS actual signed division MIN-result control WORD" + str(width), flush=True)



if __name__ == "__main__":
    main()
