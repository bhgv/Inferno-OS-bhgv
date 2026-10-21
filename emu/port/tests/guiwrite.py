#!/usr/bin/env python3
"""Exercise production guiwrite parsing with a minimal Tk/Dis boundary."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent
source = (root.parent / "devgui.c").read_text()
start = source.index("static long\nguiwrite(")
end = source.index("\n\nstatic int\nguiwstat(", start)
harness = (root / "guiwrite.c").read_text().replace(
    "GUIWRITE_IMPLEMENTATION", source[start:end]).replace(
    "GUIRESULT_IMPLEMENTATION", source[source.index("typedef struct _rslt_string"):source.index("extern char rootdir[];")])
with tempfile.TemporaryDirectory(prefix="guiwrite-") as temporary:
    unit = Path(temporary) / "guiwrite.c"
    binary = Path(temporary) / "guiwrite"
    unit.write_text(harness)
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                   shlex.split(os.environ.get("CFLAGS", "-O1 -g")) +
                   [str(unit), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS guiwrite bounded input, immutable input, command limit and cleanup")
