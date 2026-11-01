"""Exercise actual mk command-line assignments with valid and refused TMPDIR values."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


# Run an explicit mk binary in a private working directory with a chosen TMPDIR.
# Return captured status/output; timeout is an active failure, never a success oracle.
def invoke(executable, work, temporary):
    return subprocess.run(
        [str(executable), "-f", "recipe.mk", "ANSWER=42", "show"],
        cwd=work, env=dict(os.environ, TMPDIR=str(temporary)),
        capture_output=True, text=True, timeout=20,
    )


# Own all fixture files; argv supplies an already source-built mk, never a hidden SDK.
# Verify assignment parsing, spaced directories, cleanup and graceful refusal when
# the configured directory cannot be used. No global /tmp or user files are changed.
def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: mk-tempdir.py /absolute/path/to/mk")
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="mk assignment ") as directory:
        work = Path(directory)
        temporary = work / "temporary files"
        temporary.mkdir()
        (work / "recipe.mk").write_text("show:V:\n\techo RESULT $ANSWER\n")
        for _ in range(2):
            result = invoke(executable, work, temporary)
            if result.returncode != 0 or "RESULT 42" not in result.stdout:
                raise RuntimeError("valid TMPDIR assignment failed: " + result.stdout + result.stderr)
            if list(temporary.iterdir()):
                raise RuntimeError("mk retained a consumed command-line assignment file")
        for refused in (work / "missing", work / ("x" * 4096)):
            result = invoke(executable, work, refused)
            if result.returncode <= 0 or not result.stderr:
                raise RuntimeError("unusable TMPDIR was ignored or crashed: " + result.stdout + result.stderr)
        if (work / "missing").exists():
            raise RuntimeError("mk unexpectedly created the refused directory")
    print("PASS mk TMPDIR: actual command-line assignments, spaced paths, cleanup and two graceful refusals")


if __name__ == "__main__":
    main()
