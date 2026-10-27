# CALL/MCALL target spans on AMD64 and ARM64

CALL previously published nil/H targets after overwriting the caller's frame
links. CALL and MCALL also accepted bytecode PCs whose complete `Inst` extent
wrapped the native address space. The next interpreter fetch could therefore
fail after the call had already changed registers and ownership.

The C handlers and both priority emitters now reject these cases before frame,
register or reference-count publication. CALL reads the current module's
`compiled` flag only for a target that cannot hold a complete bytecode
instruction; ordinary targets retain their existing dependency-free path.
MCALL uses the already captured callee mode. Nonzero legacy compiled modes
retain opaque native PC tokens, which must not be treated as `Inst` objects.
Nil and H remain invalid in both modes.

The emitters require a positive instruction stride no greater than 2047 and
reject invalid CALL module/compiled-field layouts without publishing code.
These are numeric span checks, not proofs of allocation membership or executable
mapping. No synthetic high-address destination is executed by these tests.

## Validation

Run from the repository root; the guest drivers require Unicorn and Capstone:

```sh
python3 libinterp/tests/control-transfer-native.py
CONTROL_NO_ERRORS=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/control-transfer-native.py
python3 libinterp/tests/control-transfer-jit.py
CONTROL_INST_BYTES=1 python3 libinterp/tests/control-transfer-jit.py
CONTROL_INST_BYTES=2047 python3 libinterp/tests/control-transfer-jit.py
python3 libinterp/tests/mcall-jit.py
python3 libinterp/tests/mcall-native.py
python3 libinterp/tests/call-return-native.py
python3 libinterp/tests/module-transition-native.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-dispatch-jit.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The native control driver now selects AMD64 or ARM64 from the host and accepts
`CONTROL_WORD_BITS` to isolate either scalar model. This series ran natively on
AMD64; ARM64 machine code ran in Unicorn, not on a physical ARM64 host.

| Matrix | Result |
| --- | --- |
| Real C/fast-plus-C/complete RX control transfers | 21,504 full-image routes per WORD model |
| Invalid control transfers | 734 WORD32 and 742 WORD64 routes |
| Import/export lookup | 2,608 checks per WORD model |
| Legacy native-PC and last complete bytecode-span construction | 438 routes per WORD model |
| Guest control code, stride 1 | 15,040 cases per architecture |
| Guest control code, stride 24 | 17,696 cases per architecture |
| Guest control code, stride 2047 | 147,168 cases per architecture |
| MCALL independent image/write/FP oracle | 9,216 cases per architecture |
| Native MCALL ownership/fallback | 24,576 cases per WORD model |
| Native MCALL/RET round trips | 6,144 cases per WORD model |
| Native complete dispatch | 177,152 routes, 5,002 compiler and 295 integration checks per WORD model |
| ARM64 compiler preflight | 5,002 checks per WORD model |
| Guest complete tails | 16,881 cases per architecture |

The control guest oracle checks unchanged VM images and an empty write trace
on refusal, byte-aligned operand/frame cells, independent layout biases, the
exact last accepted instruction span, malformed current-module prefixes,
native-token compatibility and bounded output publication. ABI rejection
checks are additional to the execution counts above. Valid native control
routes also pass ASan/UBSan with NDEBUG; error routes use UBSan and real VM
error-boundary longjmps. The module-transition ASan/UBSan suite still passes
172,032 LOAD and 393,216 builtin-call routes plus its negative matrices.

Ten independent probes reject the parent implementation: nil CALL, wrapping
CALL and wrapping MCALL through C under each WORD model, plus CALL and MCALL
machine code independently on AMD64 and ARM64. Use `CONTROL_SOURCE` for a parent
`xec.c`; `CONTROL_SECTION=invalid`, `CONTROL_BAD=13` (nil CALL) or `14`
(wrapping target), and `CONTROL_OP=4`/`9` select CALL/MCALL. Guest probes accept
`CONTROL_EMITTER_DIR`, `CONTROL_ARCHES`, `CONTROL_GUEST_OP=0`/`1` and
`CONTROL_SKIP_ABI=1` to isolate execution from layout-rejection probes.

## Readiness

| Architecture | Complete JIT entries, including C | Guarded native fast paths |
| --- | --- | --- |
| AMD64 | 173/173 (100%) | 166/173 (95.95%) |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) |

The opcode counts are unchanged. Seven complex operations retain C handlers.
This does not resolve the compiler/frame/GC pointer-layout blockers documented
in [README.native-bootstrap.md](../../utils/tests/README.native-bootstrap.md),
and does not establish a bootable 64-bit VM. RV64, Tk, security/crypto and the
MIPS/SPARC/PowerPC JIT sources remain outside this series.
