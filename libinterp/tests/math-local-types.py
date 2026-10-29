#!/usr/bin/env python3
"""Compile actual Math guards against local/canonical WORD32/64 descriptors."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning Math implementation and fixture.
SOURCE = Path(os.environ.get("MATH_LOCAL_SOURCE", ROOT / "libinterp/math.c"))


# Extract one actual production function; source/signature are borrowed strings.
# Return copied C text with comments/literals excluded from brace-depth accounting.
def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for token in re.finditer(
        r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]
    ):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                return source[start : brace + token.end()]
    raise ValueError(signature)


# Build both bounded models with explicit predicates and sanitizer error failure.
# No arguments; environment may select a parent source for regression proof.
def main():
    source = SOURCE.read_text()
    signatures = ["static int\nmatharraycount(", "static int\nmathvectorcount("]
    if "static int\nmatharraytype(" in source:
        signatures.insert(0, "static int\nmatharraytype(")
    implementation = "\n\n".join(function(source, name) for name in signatures)
    unit = (
        (ROOT / "libinterp/tests/math-local-types.c")
        .read_text()
        .replace("IMPLEMENTATION", implementation)
    )
    with tempfile.TemporaryDirectory(prefix="math-local-types-") as directory:
        root = Path(directory)
        fixture = root / "fixture.c"
        fixture.write_text(unit)
        cc = shlex.split(os.environ.get("CC", "cc"))
        flags = shlex.split(
            os.environ.get(
                "CFLAGS", "-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all"
            )
        )
        widths = tuple(map(int, os.environ.get("MATH_LOCAL_WORD_BITS", "32 64").split()))
        if not widths or any(width not in (32, 64) for width in widths):
            raise ValueError("MATH_LOCAL_WORD_BITS must select 32 and/or 64")
        for width in widths:
            binary = root / ("types" + str(width))
            subprocess.run(
                cc
                + flags
                + [
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Wno-sign-compare",
                    "-DWORD_BITS=" + str(width),
                    str(fixture),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
