#!/usr/bin/env python3
"""Execute the production C big-constant interning function with native host formatting widths."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning Inferno source root.


# Extract one actual compiler function while ignoring braces in comments and literals.
# source/signature are borrowed text. Return its complete definition or fail explicitly.
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
            if depth == 0:
                return source[start : brace + token.end()]
    raise ValueError("unterminated compiler function")


# Optional ECOM_SOURCE replays an earlier source; CC/CFLAGS select actual host tools.
# Compile/run the retained function under UBSan by default, returning only after identity checks pass.
def main():
    source = Path(os.environ.get("ECOM_SOURCE", ROOT / "limbo/ecom.c")).read_text()
    body = function(source, "Decl*\nglobalBconst(")
    template = Path(__file__).with_suffix(".c").read_text()
    with tempfile.TemporaryDirectory(prefix="big-constant-host-") as temporary:
        directory = Path(temporary)
        unit = directory / "identity.c"
        unit.write_text(template.replace("/* RETAINED_GLOBAL_BCONST */", body))
        binary = directory / "identity"
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(
            os.environ.get("CFLAGS", "-O1 -g -fsanitize=undefined -fno-sanitize-recover=all")
        )
        subprocess.run(command + ["-std=c99", str(unit), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)


if __name__ == "__main__":
    main()
