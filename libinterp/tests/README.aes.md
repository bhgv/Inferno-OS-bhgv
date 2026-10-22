# AES setup, CBC and native-state regressions

Run from the repository root:

```sh
python3 libinterp/tests/aes.py
AES_COMPACT=1 python3 libinterp/tests/aes.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer' python3 libinterp/tests/aes.py
```

The driver requires the Python cryptography package for an independent AES
primitive. Each Keyring/Crypt module passes 1,813 cases in native WORD64 and
WORD32 scalar models. The default FULL_UNROLL branch passes UBSan and ASan+UBSan;
the compact native AES branch separately passes UBSan. Generated builtin frames and native
Heap/Array/AESstate declarations are real; bounded VM heap, reference validation
and exception delivery are fixture models. WORD32 retains native pointers.

Independent vectors use 128/192/256-bit keys, deterministic plaintext lengths
0 through 129 and a fixed nonzero IV. The reference ECB primitive additionally
matches the FIPS-197 AES-128 block vector. CBC encryption/decryption is checked
at every aligned split point, including the resulting IV and guarded payload.
The repository's existing partial-tail convention is explicitly modeled: after
complete CBC blocks, encrypt the IV and XOR the remaining bytes. That convention
requires matching packet boundaries and is preserved rather than replaced by
padding or a block-size restriction.

Setup validates full VM key/IV metadata before allocating or releasing the old
result. Invalid key sizes, types, impossible lengths, missing data, short IVs,
nil keys and VM heap failure preserve the previous owner. CBC validates complete
counts before native int conversion and must leave state/payload intact on
rejection. Empty non-nil arrays may have null data; zero-byte operations retain
the IV. Existing nil-buffer no-op and module-specific nil-key errors remain.

Native setup copies possibly aliased key and IV bytes before clearing its
state. Rekeying from the state's own buffers is checked for all three key sizes.
The historical native truncation of keys longer than 32 bytes is retained;
other unsupported sizes and missing keys fail through sysfatal before state
mutation. Native negative or nonempty-null CBC spans also fail before mutation.
A fixture sysfatal hook checks preservation and does not imply recoverability
from fatal errors in production. Byte substitution tables are promoted to u32
before shifts into bit 31, in both encryption/decryption implementations.

Baseline wrapper tests reproduce loss of old results. Native baseline probes
reproduce signed-shift UBSan and, after isolating that defect, incorrect rekeying
when key/IV alias the state. VM/type reference counts must balance after each
case. Full Inferno boot, actual builtin/JIT ABI, GC scheduling and hardware AES
backends are outside this host fixture. Active state layouts and opcodes do not
change. ASan stack instrumentation remains enabled; LeakSanitizer inspection is
disabled for the sandbox, with explicit allocation accounting retained.
