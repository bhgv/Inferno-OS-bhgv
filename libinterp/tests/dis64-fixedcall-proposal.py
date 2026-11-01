#!/usr/bin/env python3
"""Check proposed first fixed-call bytes through actual existing codecs, without calling."""
import ctypes
import json
import os
from pathlib import Path
import runpy
import shlex
import struct
import subprocess
import tempfile


# Load literal proposal bytes and reuse owning C probe wrappers, not their expected
# values. Test canonical emission, exact structural comparison and refusal atomicity.
# CC/CFLAGS are explicit host inputs; no target emulator or file admission is used.
def main():
    directory = Path(__file__).resolve().parent
    fixture = json.loads((directory / "dis64-fixedcall-proposal.json").read_text())
    probe = runpy.run_path(str(directory / "dis64-storage.py"))
    signature = bytes.fromhex(fixture["signature_hex"])
    imports = bytes.fromhex(fixture["imports_hex"])
    pool = bytes.fromhex(fixture["pool_hex"])
    if signature != struct.pack(">IIII", 1, 3, 1, 2):
        raise RuntimeError("proposal signature is not Real(Big)")
    if imports != struct.pack(">IIII", 1, 1, 2, 16) + signature:
        raise RuntimeError("proposal import record changed")
    expected_pool = struct.pack(">I", 3)
    for name in ("", "$Math", "bits64real"):
        expected_pool += struct.pack(">I", len(name))
        expected_pool += b"".join(struct.pack(">I", ord(character)) for character in name)
    if pool != expected_pool:
        raise RuntimeError("proposal Rune32 pool changed")
    with tempfile.TemporaryDirectory(prefix="fixed-call-proposal-") as temporary:
        work = Path(temporary)
        source = work / "probe.c"
        binary = work / "probe.so"
        source.write_text(probe["SOURCE"])
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get(
            "CFLAGS", "-O2 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all"
        ))
        command += ["-std=c99", "-Wno-unused-function", "-shared", "-fPIC",
                    "-I" + str(probe["ROOT"] / "include"), str(source), "-o", str(binary)]
        subprocess.run(command, check=True)
        lib = ctypes.CDLL(str(binary))
        uintp = ctypes.POINTER(ctypes.c_uint)
        lib.signaturewrite.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                                       uintp, ctypes.c_uint, uintp]
        lib.signatureequal.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p,
                                       ctypes.c_uint, ctypes.POINTER(ctypes.c_int)]
        lib.bindingsprobe.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                                     ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p,
                                     ctypes.c_uint, ctypes.c_uint, uintp]
        parameters = (ctypes.c_uint * 1)(2)
        for capacity in range(21):
            output = ctypes.create_string_buffer(b"Z" * 32, 32)
            written = ctypes.c_uint(12345)
            result = lib.signaturewrite(output, capacity, 3, parameters, 1, ctypes.byref(written))
            expected = signature + b"Z" * 16 if capacity >= 16 else b"Z" * 32
            if result != int(capacity >= 16) or output.raw != expected:
                raise RuntimeError("signature writer changed bytes or refusal canaries")
            if written.value != (16 if capacity >= 16 else 12345):
                raise RuntimeError("signature writer changed refused extent")
        for parameter in (1, 2, 3, 4):
            other = struct.pack(">IIII", 1, 3, 1, parameter)
            equal = ctypes.c_int(9)
            if lib.signatureequal(signature, 16, other, 16, ctypes.byref(equal)) != 1:
                raise RuntimeError("canonical scalar comparison refused")
            if equal.value != int(parameter == 2):
                raise RuntimeError("same-length signatures confused scalar kinds")
        cases = [(imports, True)]
        cases += [(imports[:length], False) for length in range(len(imports))]
        for offset, value in ((4, 3), (8, 3), (12, 15), (16, 2), (28, 9)):
            malformed = bytearray(imports)
            malformed[offset:offset + 4] = struct.pack(">I", value)
            cases.append((bytes(malformed), False))
        for payload, valid in cases:
            fields = (ctypes.c_uint * 6)(*[12345] * 6)
            result = lib.bindingsprobe(payload, len(payload), 0, pool, len(pool),
                                       b"\0\0\0\0", 4, 0, fields)
            expected = [1, 2, 0, 0, 3, 1] if valid else [12345] * 6
            if result != int(valid) or list(fields) != expected:
                raise RuntimeError("import codec admission/refusal publication changed")
        # Inferno uses content-equivalent names at ordinals0/1; ordinal numbers
        # are local to a pool, not shared semantic identities or signature hashes.
        peer = directory / "dis64/fixed-math"
        peer_signature = bytes.fromhex((peer / "signature.hex").read_text())
        peer_pool = bytes.fromhex((peer / "strings.hex").read_text())
        peer_imports = bytes.fromhex((peer / "imports.hex").read_text())
        if peer_signature != signature:
            raise RuntimeError("peer canonical signature differs")
        fields = (ctypes.c_uint * 6)(*[12345] * 6)
        if lib.bindingsprobe(peer_imports, len(peer_imports), 0, peer_pool,
                             len(peer_pool), b"\0\0\0\0", 4, 0, fields) != 1:
            raise RuntimeError("peer literal import/pool refused")
        if list(fields) != [0, 1, 0, 0, 3, 1]:
            raise RuntimeError("peer local ordinals or scalar signature changed")
    print("PASS fixed-call proposal: literal Real(Big), Rune pool, import record, canaries; no call executed")


if __name__ == "__main__":
    main()
