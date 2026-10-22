# RC4 wrapper and stream-state regressions

Run from the repository root:

```sh
python3 libinterp/tests/rc4.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libinterp/tests/rc4.py
```

Keyring and Crypt each pass 11,129 cases in native WORD64 and WORD32 scalar
models under UBSan and ASan+UBSan. The test uses actual generated builtin frames,
Heap/Array/RC4state declarations and production setup/update/skip/back routines.
Only VM heap allocation, state-reference validation and error boundaries are
bounded fixture models. WORD32 changes only WORD/UWORD while keeping pointers
native; it is not execution of a complete 32-bit Dis ABI.

A separate Python implementation generates reference streams for five keys of
lengths 1, 3, 5, 16 and 256. The five-byte key's first 16 bytes are additionally
checked against the RFC 6229 vector. Every split point is exercised for message
lengths 0, 1, 15, 16, 63, 64, 127, 128, 255, 256, 257 and 512. Checked output guards
follow the exact byte-array payload. For every skip count 0 through 512, the
next stream bytes must match the oracle, and backing up must restore the entire
original permutation and indices byte for byte.

Setup validates full-width byte-array metadata and nonempty seeds before heap
allocation. VM allocation failure and invalid keys preserve the previous result.
Keyring retains its historical nil-seed-to-nil behavior, while Crypt retains a
nil-reference error; that error now preserves the old result. Native setup
rejects nil/empty seeds through sysfatal before changing the permutation.

Update/skip/back validate full VM counts before narrowing to native int.
Negative and too-wide counts raise VM bounds errors and leave state/payload
unchanged. Updates retain their existing nil-buffer no-op behavior. Native
negative counts use sysfatal; a zero-byte native update returns before forming
a byte span, and a nonempty update requires a data pointer. A fixture sysfatal
hook is used only to verify invalid native inputs do not mutate their state;
it does not imply recoverability from native fatal errors in production.

The shared private byte-array helper in each module is renamed from digestarray
to bytearray and reused by digest and stream wrappers. Digest regressions remain
part of validation. No generated interfaces, state layouts, stream outputs or
JIT interfaces change. VM references and type counts must balance after every
case. Baseline wrappers fail rejected-count checks; native baseline setup reads
through an invalid zero-length seed under ASan.

These are host stream, bounds and ownership tests. Full Inferno boot, actual
builtin/JIT calling conventions and GC marking/scheduling remain outside scope.
ASan stack instrumentation is enabled. LeakSanitizer inspection is disabled for
the sandbox; explicit fixture allocation accounting remains mandatory.
