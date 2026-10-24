# Frame-based fixed JIT and VM slot publication

This series fixes unaligned operands and stores in seventeen VM handlers:
mulx/divx, mulx0/divx0/cvtxx0, mulx1/divx1/cvtxx1, movmp, new/newz/mnewz,
frame/mframe, newa/newaz and newcmp. Fixed coefficients and scale values are
captured from actual Stmp/Dtmp byte slots before any overlapping result store.
Type indices, module pointers, array lengths and replaced owner pointers are
likewise captured without typed-pointer alignment assumptions. Bounds checks,
zero-divisor errors, zero shortcuts, reference ownership and publication order
are retained. The existing aligned cvtxx is only regrouped for readability.

## Native mulx/divx

x86-64, ARM64 and RV64M now execute IMULX/IDIVX with signed 64-bit intermediates,
modular multiplication/left shifts, arithmetic right shifts and final VM WORD
truncation. The full scale range -63..63 is supported. Invalid scales, nil/H
frames and zero/-1 divisors decline without VM-visible writes; C retains the
exact error precedence and minimum/-1 semantics. RV64 requires M, while RV32 and
no-M targets decline both opcodes. RV64 also declines misaligned frame pointers
and malformed temporary offsets before a naturally misaligned memory access.

The private emitter layout now receives REG.FP and the fixed temporary offset
from each compiler adapter. The temporary retains its existing Dis offset;
this does not migrate frames, calls, GC maps or the canonical Dis word format.
No native stack, helper calls, FP registers or executable heap policy changes
are introduced. Code uses the existing caller-saved scratch-register contract.

## Checks

- `LEAF64_CASE=framefixed python libinterp/tests/leaf64-jit.py` passed 80,922
  machine-code/ABI/alias/oracle checks over all three targets and both word sizes.
  This run includes every scale for representative pairs, signed extrema,
  random operands, source/middle aliases, overlap in both directions, destination
  overlap with the scale slot, inaccessible rejected operands and nil/H frames.
  The final RV64 version additionally rejects unaligned frame pointers.
- `LEAF64_CASE=frameabi` passes 670 encoder/capacity/layout checks, including
  unchanged output on malformed frame descriptions. Future combined runs include
  these additional 64 layout assertions in the framefixed total.
- `riscv-jit.py` passes 2,077 checks including no-M rejection. The production
  compiler harness publishes the two new leaves on all three backends; its host
  RV64M compilation uses an explicit simulated M feature define without executing
  target instructions on x86-64.

Unicorn/Capstone must be on PYTHONPATH for emitter instruction execution.

`fixed-frame.py` extracts actual VM helpers and handlers. It checks independent
source, middle and frame byte offsets 0..7, destination aliases and overlap with
frame temporaries: 9,400,320 WORD64 and 8,847,360 WORD32 cases (18,247,680 total).
The production x86-64 `leaf64-runtime.py` executes the same matrix through real
published native entries and then actual C helpers. Both models pass ASan/UBSan,
alongside all previously added arithmetic and lifetime checks.

`fixed-ops.py` retains its independent Python arbitrary-precision oracle:
37,800 cases per word model, now with varying alignment. Its native executable
covers 1,625,400 boundary/alignment/error/alias cases per model under ASan/UBSan,
including both temporary slots as destinations. Whole-buffer snapshots check
that errors change no bytes and successful operations touch only the result.

`module-types.py` checks independent source/middle/destination byte offsets for
all consumers: 327,884 type/call cases across two models pass ASan/UBSan. Calls
run in a fresh process so independent longjmp families do not share ASan shadow
state. `STACK_CASES=framealignment stack-startup.py` passes 8,192 new ASan/UBSan
cases across both scheduler implementations and both word models, covering inline
and extend/unextend paths. The complete stack fixture passes 8,896 UBSan cases.
The complete stack ASan run is not claimed: root-startup reports a diagnostic at
krethrow/siglongjmp in this environment, reproduced with parent 77a1a89 as well.
Stack instrumentation remains enabled.

The core environment suite passes 38 host drivers. Parent-source substitution
in FIXED_FRAME_SOURCE, MODULE_TYPES_XEC_SOURCE and STACK_XEC_SOURCE reproduces
unaligned result/pointer loads/stores on 77a1a89. Hardware ARM64/RV64 execution,
complete Inferno boots and native-frame/call/return JIT work remain untested or
outside this decoded-operation leaf implementation. Excluded backends and
subsystems are unchanged.
