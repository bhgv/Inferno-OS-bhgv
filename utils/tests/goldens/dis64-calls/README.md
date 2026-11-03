# Immutable private Dis64 scalar-call goldens

These are existing Dis64 v1.0 / ABI1 containers, not a new format or executable
file-loader permission. Decode each `.hex` with Python `bytes.fromhex`; SHA256
and exact byte lengths are recorded in `manifest.json`. Hashes identify bytes;
they authenticate neither input nor compiler declarations.

The manifest's `locals` are trusted fixture/compiler declarations supplied to the
private validator. They are NOT derived from untrusted input and cannot become
file authority. The exported root is ordinal0, entry0, frame0, signature `() ->
void`. Helpers are ordinal1 (and2 for chain42), frame1, signature `(int32) ->
int32`; exact entry PCs and numeric local declarations are in the manifest.
Frame0 is112 bytes, frame1 is88 bytes, aligned8 with zero pointer maps. The
caller-owned reserved result tail is byte104; ordinary WORD32 LEA is unchanged.
For chain42 the root calls two leaves SEQUENTIALLY, copying the first returned
value before its second FRAME; helpers never call helpers.

`answer42` returns42; `chain42` returns42 twice; `reordered42` permutes legal
section order. `divide-zero` passes static validation but the actual callee raises
division by zero before result publication. Other files must refuse before Module
allocation and preserve the previous proof records, count and borrowed image
view. Call-role mutations retain valid envelope framing; truncated/trailer cases
intentionally test framing. No checksum extension is present in this format.

Run `python3 utils/tests/dis64-call-goldens.py`. The normal test verifies committed
bytes without rewriting, materializes real `.dis64` host files, reads them with
C stdio and validates all sections from ONE borrowed image. Only then does the
private trusted constructor lower into actual owning interpreter/frame/heap/JIT
bodies. C/RX, frame extension, saved registers, quanta1/7/1000, allocation refusal
and callee error cleanup are checked. Explicit bounded host transports remain;
this is neither Inferno namespace readmod/parsemod nor AWM/device acceptance.
The production semantic gate is separately required to refuse accepted fixtures.

Five actual private-header mutations require exact semantic exit10: module/frame
binding, executable profile, atomic publication, scalar maps and type population.
Signals, compiler/sanitizer errors and timeouts do not count as successful controls.
Use `--write-goldens` only when deliberately reviewing a corpus revision.

FlashVM may inspect these identical bytes and report its present executable
refusal. Shared-codec inspection is not independent whole-file execution proof.
Keep public FRAME/CALL/RET/LEA admission closed until independent validators,
untrusted local-kind authority and actual lifecycle/error execution agree.
