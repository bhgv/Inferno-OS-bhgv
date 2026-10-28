# ARM64 list/channel runtime-call target checks

This continues [complete-entry target checks](README.jit-target-alignment.md)
inside native fast bodies. ARM64 now rejects unaligned addresses for all six
runtime helper identities before their call sequence is staged:

| Layout identity | Commands using that identity |
| --- | --- |
| consentry | CONSB, CONSW, CONSL, CONSF, CONSM, CONSMP, CONSP |
| constypeentry | CONSMP |
| channelentry | NEWCB, NEWCW, NEWCL, NEWCF, NEWCP |
| rawchannelentry | NEWCM |
| mappedchannelentry | NEWCMP |
| channeltransferentry | SEND, RECV |

The six affected emitter helpers are a64scalarcons, a64mappedcons, a64rawcons,
a64pointercons, a64channel and a64transfer. AMD64 retains byte-addressable C
function identities. Unused callback fields do not reject unrelated commands.
Existing null/H rejection remains, and valid aligned identities retain identical
emission. Numeric target checks cannot prove executable mapping ownership.

`EMIT_LAYOUT_CASE=callbacks python3 libinterp/tests/emitter-layouts.py` runs
29,064 callback/layout cases: six fields, all 173 active commands, seven identity
patterns, two architectures and two WORD models. It checks sizing, caller-output
canaries, unchanged rejected output and unchanged layout input. Target patterns
include nil, H, all three bad low-bit patterns, a high aligned identity and the
highest aligned numeric address. The complete default emitter-layout fixture
also retains the preceding integer-offset/publication/span checks.

Nine isolated probes with the unchanged f77c625 ARM64 header fail the new
semantic predicates: scalar/raw/pointer/mapped lists, mapped descriptor lookup,
scalar/raw/mapped channels and buffered transfer. Set EMIT_LAYOUT_HEADERS,
EMIT_LAYOUT_CALLBACK_FIELD and EMIT_LAYOUT_CALLBACK_OP to replay a specific
source/identity/opcode. Test compilation failures are not regression evidence.

The scalar-cons, payload-cons, mapped-cons, scalar-channel, payload-channel and
buffered-transfer Unicorn drivers pass on both architectures: 1,031,770 total
ABI/image/guard/ownership/FP checks. Native AMD64 complete-dispatch checks and
ARM64 compiler-only checks also pass in both WORD models. The foreign ARM64
fixtures now explicitly align borrowed callback declarations: GCC can leave
cold abort-only AMD64 stubs byte-aligned despite -falign-functions=4. These
identities model AArch64 targets; foreign machine code is never executed by the
AMD64 compiler fixture. Publication/failure/retry tests pass for both WORD models.

The accompanying [Limbo AST transformation series](../../limbo/tests/README.node-transforms.md)
removes three more depth-proportional compiler stacks. No Tk, security/crypto,
MIPS/SPARC/PowerPC JIT or RV64 production source changes are included.

| Architecture | Complete native+C JIT entries | Commands with native fast paths | C-only entries |
| --- | ---: | ---: | ---: |
| x86-64 | 173/173 (100%) | 166/173 (95.95%) | 7 |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) | 7 |

The [per-command table](README.jit-readiness.md) remains valid and is checked
against actual production sizing emitters. These are entry-availability metrics;
physical ARM64 execution and coordinated compiler/loader/frame/GC ABI acceptance
remain outstanding before claiming a complete bootable native 64-bit VM.

The leaf64-runtime fixture must also explicitly align its abort-only `cjitexec`
identity in the copied interface header. With unsanitized `CFLAGS=-O1 -g`, the
x64 host may place that cold stub at an odd address even when the six list/channel
helpers are aligned. This is a fixture-model correction: the production ARM64
alignment guard remains active, and no host code is executed as AArch64.
Both WORD32 and WORD64 publication paths are checked with and without UBSan.
