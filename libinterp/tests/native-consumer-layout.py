#!/usr/bin/env python3
"""Report representative native-layout blockers; never certify a complete ABI."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning Inferno source checkout.


# Compile actual declarations with independent scalar selections. header is an
# explicit generated file or the checked-in legacy header; all temporary files
# belong to this invocation. Return measured JSON rows, not an ABI readiness flag.
def measure(header):
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(
        os.environ.get(
            "CFLAGS", "-O1 -g -fsanitize=undefined -fno-sanitize-recover=all"
        )
    )
    includes = [
        "-I" + str(ROOT / path)
        for path in ("Linux/386/include", "emu/port", "include", "libinterp")
    ]
    rows = []
    with tempfile.TemporaryDirectory(prefix="native-consumer-layout-") as temporary:
        work = Path(temporary)
        shutil.copyfile(header, work / "audited-runt.h")
        for name, selection in (
            ("legacy", []),
            ("WORD32", ["-DDIS_WORD32=1"]),
            ("WORD32-file", ["-DDIS_WORD32=1", "-DDIS64_PILOT_LOADER=1"]),
        ):
            binary = work / name
            subprocess.run(
                compiler
                + flags
                + selection
                + ["-I" + str(work)]
                + includes
                + [
                    str(ROOT / "libinterp/tests/native-consumer-layout.c"),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            row = json.loads(subprocess.check_output([str(binary)], text=True))
            row["selection"] = name
            row["scalar_model_matches"] = (
                row["word"] == 4 and row["pointer"] == 8 and row["frame_links"] == 32
            )
            row["aggregate_sizes_match"] = all(
                row[base + "_actual"] == row[base + "_declared"]
                for base in ("qid", "dir", "connection")
            )
            row["frame64_offsets_match"] = (
                row["sleep_result"] == 32 and row["sleep_argument"] == 64
            )
            row["representative_checks_match"] = (
                row["scalar_model_matches"]
                and row["aggregate_sizes_match"]
                and row["frame64_offsets_match"]
            )
            rows.append(row)
    return rows


# Explicitly distinguish successful measurement from a compatible native ABI.
# --require-coherent returns2 for a measured incompatibility; compilation/run
# failures propagate separately. No flags, wire constants or headers are changed.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--header",
        type=Path,
        default=ROOT / "libinterp/runt.h",
        help="Explicit limbo -a or experimental -H header; default is the checked-in legacy declaration",
    )
    parser.add_argument(
        "--require-coherent",
        action="store_true",
        help="Exit2 if any representative WORD32 check fails; passing is still not full ABI certification",
    )
    args = parser.parse_args()
    rows = measure(args.header.resolve())
    matching = all(
        row["representative_checks_match"]
        for row in rows
        if row["selection"] != "legacy"
    )
    print(
        json.dumps(
            {
                "header": str(args.header.resolve()),
                "representative_checks_match": matching,
                "full_native_abi_verified": False,
                "measurements": rows,
            },
            indent=2,
        )
    )
    if args.require_coherent and not matching:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
