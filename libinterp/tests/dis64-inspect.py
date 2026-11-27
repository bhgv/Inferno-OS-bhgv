#!/usr/bin/env python3
"""Check the real inspector's opt-in semantic gate and untouched rejection output."""
from pathlib import Path
import json
import os
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Run a literal argv with bounded time and require exactly the expected status.
# The tool reports structure only: no fixture authorizes VM execution.
def call(arguments, good=True):
    result = subprocess.run([str(a) for a in arguments], capture_output=True, text=True, timeout=20)
    if (result.returncode == 0) != good:
        raise RuntimeError(str(arguments) + "\n" + result.stdout + result.stderr)
    return result


# Exercise exact dependency, symbol and signature comparisons across two Rune pools.
def import_matches(binary, work):
    raw = runpy.run_path(str(ROOT / 'utils/tests/dis64-storage-image.py'))
    image, sections, words = raw['image'], raw['sections'], raw['words']
    binding, signature, descriptor = raw['binding'], raw['signature'], raw['descriptor']
    consumer, provider = (work / name for name in ('consumer.dis64', 'provider.dis64'))

    # Serialize distinct pools so ordinal equality cannot establish a match.
    def with_pool(parts, names):
        pool = words(len(names)) + b''.join(words(len(name), *name) for name in names)
        return image([(kind, pool if kind == 7 else data) for kind, data in parts])

    module = (0x44, 0, 0xffffffff)
    symbol = (0x66, 0x301)
    other = (0x67,)
    consumer_names = [(0x43,), module, symbol, module, symbol]
    provider_names = [other, symbol, module]
    types = [descriptor(64, 8), descriptor(80, 8)]
    code = words(2) + bytes((15, 27, 15, 27))
    checks = 0
    for result_kind in range(5):
        for params in ((), (1,), (4, 3, 2, 1)):
            sig = signature(result_kind, params)
            imports = [binding(module=1, symbol=2, sig=sig), binding(module=3, symbol=4, sig=sig)]
            exports = [binding(export=True, symbol=0), binding(export=True, symbol=1, entry=1, frame=1, sig=sig)]
            consumer.write_bytes(with_pool(sections(imports=imports), consumer_names))
            provider.write_bytes(with_pool(sections(profile=0, databytes=0, name=2, code=code, types=types, exports=exports),
                                           provider_names))
            for ordinal in (0, 1):
                report = json.loads(call([binary, consumer, '--match-import', ordinal, provider]).stdout)
                expected = dict(import_ordinal=ordinal, export_ordinal=1, entry_pc=1,
                                frame_type=1, execution_supported=False)
                if report != expected:
                    raise RuntimeError('wrong cross-pool import coordinates')
                checks += 1

    sig = signature(1, (1, 2))
    consumer.write_bytes(with_pool(sections(imports=[binding(module=1, symbol=2, sig=sig)]), consumer_names))
    valid = binding(export=True, symbol=1, entry=1, frame=1, sig=sig)
    wrong_signature = [signature(2, (1, 2)), signature(1, (2, 1)), signature(1, (1,))]
    bad_exports = [[binding(export=True, symbol=1, sig=value)] for value in wrong_signature]
    bad_exports += [[], None, [binding(export=True, symbol=0)],
                    [binding(export=True, symbol=1, entry=2, sig=sig)],
                    [binding(export=True, symbol=1, frame=2, sig=sig)], [valid, valid]]
    for exports in bad_exports:
        provider.write_bytes(with_pool(sections(profile=0, databytes=0, name=2, code=code, types=types, exports=exports),
                                       provider_names))
        refused = call([binary, consumer, '--match-import', 0, provider], False)
        if refused.returncode != 1 or refused.stdout:
            raise RuntimeError('provider rejection published coordinates')
        checks += 1

    # A provider can be structurally valid and still name another dependency.
    provider.write_bytes(with_pool(sections(profile=0, databytes=0, name=0, code=code, types=types, exports=[valid]),
                                   provider_names))
    if call([binary, consumer, '--match-import', 0, provider], False).returncode != 1:
        raise RuntimeError('dependency name mismatch was not rejected')
    checks += 1
    provider.write_bytes(with_pool(sections(profile=0, databytes=0, name=2, code=code, types=types, exports=[valid]),
                                   provider_names))
    for ordinal, status in ((1, 1), (4294967295, 1), (4294967296, 2), ('-1', 2),
                            ('+1', 2), ('', 2), ('1x', 2), (' 0', 2)):
        refused = call([binary, consumer, '--match-import', ordinal, provider], False)
        if refused.returncode != status or refused.stdout:
            raise RuntimeError('invalid import ordinal was not rejected')
        checks += 1
    consumer.write_bytes(with_pool(sections(imports=[]), consumer_names))
    if call([binary, consumer, '--match-import', 0, provider], False).returncode != 1:
        raise RuntimeError('empty import table was not rejected')
    checks += 1
    print(f'PASS Dis64 import inspector: {checks} cross-image checks')


# Build the actual upstream CLI in private storage. Mutate in-range semantic
# fields without breaking the envelope, then ensure --pilot rejects before I/O.
def main():
    golden = bytes.fromhex((ROOT / "libinterp/tests/dis64/pilot42.hex").read_text())
    with tempfile.TemporaryDirectory(prefix="dis64 inspect ") as directory:
        work = Path(directory)
        binary, image, output = (work / n for n in ("inspect", "pilot.dis64", "copy.dis64"))
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("CFLAGS", os.environ.get("NATIVE64_CFLAGS", "-O2 -Wall -Wextra -Wpedantic -Werror")))
        command += ["-std=c99", "-Wno-unused-function", "-I" + str(ROOT / "include"),
                    str(ROOT / "utils/dis64inspect/dis64inspect.c"), "-o", str(binary)]
        call(command)
        image.write_bytes(golden)
        plain = json.loads(call([binary, image]).stdout)
        report = json.loads(call([binary, image, "--pilot", "--rewrite", output]).stdout)
        if not plain["envelope_only"] or report["envelope_only"] or report["execution_supported"]:
            raise RuntimeError("semantic check misreported execution boundary")
        expected = dict(scalar_pilot=True, data_bytes=4, frame_bytes=64, entry_pc=0,
                        instruction_count=2, string_count=1, type_count=2, initializer_count=0)
        if any(report.get(k) != v for k, v in expected.items()) or output.read_bytes() != golden:
            raise RuntimeError("semantic report or rewrite differs from hand-written golden")
        initialized = bytes.fromhex((ROOT / "libinterp/tests/dis64/pilot-init42.hex").read_text())
        image.write_bytes(initialized)
        report_init = json.loads(call([binary, image, "--pilot", "--rewrite", output]).stdout)
        if report_init.get("initializer_count") != 1 or output.read_bytes() != initialized:
            raise RuntimeError("initialized pilot report/rewrite differs")
        sections = {s["kind"]: s["offset"] for s in plain["sections"]}
        mutations = [(sections[1], 1), (sections[1] + 16, 2),
                     (sections[1] + 20, 2), (sections[4], 1), (sections[3] + 8, 3)]
        for offset, value in mutations:
            bad = bytearray(golden)
            struct.pack_into(">I", bad, offset, value)
            image.write_bytes(bad)
            call([binary, image])  # Same bytes remain an accepted opaque envelope.
            output.write_bytes(b"KEEP")
            call([binary, image, "--pilot", "--rewrite", output], False)
            if output.read_bytes() != b"KEEP":
                raise RuntimeError("semantic rejection changed existing output")
        image.write_bytes(golden)
        for flags in (("--unknown",), ("--pilot", "--pilot"), ("--rewrite",),
                      ("--rewrite", output, "--pilot")):
            call([binary, image, *flags], False)
        import_matches(binary, work)
    print("PASS Dis64 inspector: real pilot metadata, opaque default, semantic rejection and rewrite")


if __name__ == "__main__":
    main()
