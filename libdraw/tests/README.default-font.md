# Built-in font bootstrap and synthetic ownership

Run `python3 libdraw/tests/default-font.py` from the repository root. The fixture
uses actual built-in bytes, getdefont, loadimage, packed metric decoding, native
subfont/weak-cache/destructor code, mkfont and freefont. Only native image
allocation/release and transport are bounded adapters; there is no renderer or
VM collector. The guarded allocator accounts for every actual native allocation.

The normal test covers all eight byte alignments, repeated loads across live
Display identities, malformed header/payload/metrics before resource allocation,
every constructor allocation refusal, packet/flush failure and reference transfer
into a synthetic Font. The last Unicode range is accepted; overflowing synthetic
ranges refuse without consuming the input. Signed bearings are checked explicitly.

`--char-model signed` and `--char-model unsigned` repeat the proof independent of
native plain-char defaults. `--control immutable`, `extent`, `pixels`, `rollback`,
`range`, `transfer` and `bearing` deliberately remove one property. Each must reach
its exact semantic stage before unsafe continuation; compiler/sanitizer/resource
failures never count as detection. The error-stop cleanup frees the fixture ledger
without invoking production destructors on deliberately invalid state. ASan/UBSan
and ordinary leak detection remain enabled in hosted jobs.

`NATIVE64_ARCH`, `NATIVE64_CFLAGS`, `NATIVE64_RUNNER`, `NATIVE64_TMPDIR` and `CC`
select the actual native target/compiler/runner. Default native compilation uses
O3 with nonrecovering UBSan. Hosted ARM64/x64 gates add ASan; the full classic-mk
pipeline includes the same normal/char-model/omission cases on its targets.
