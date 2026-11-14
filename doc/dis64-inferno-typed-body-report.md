# INFERNO-0168 — complete typed bodies with managed parameters and loops

2026-11-14. Code base 177fc56f / INFERNO-0167; peer-journal base cbe8e07e.
FLASHVM-0181/0182 were read and preserved. Their acknowledgment of our 0166/0167
is received. Three-VM forwarding, deadline/churn fixes, native threads and
concurrent GC remain peer work; none of that runtime or private wire protocol
is imported. Existing Inferno Word32/ref64 with Android ARM64 first remains
the scope, with no new public opcode/kind or managed file profile assigned.

## New complete-body lowering

`limbo/dis64-typed.c/.h` translate complete supported void functions from checked
compiler IR into direct native FP/MP operands. The type-driven frame/data plans
are consumed per opcode, preserving byte/Word/big/real widths and reference
roles. Direct moves, numeric arithmetic/conversions/comparisons, String/list/
array lengths, forward/backward branches and void returns are covered. The
frontend's shared unmarked big/real temporary storage retains its geometry;
root/scalar aliasing and incompatible reference-class moves refuse.

Branches are tied to actual PCs inside their owning function and rebased to
local instruction indexes. Code count/order stay unchanged. Header/indirect
operands, unsupported operations, result binding, handlers and generic/variadic
function conventions refuse the whole body. No partial code is published by
the opt-in report; the owned API state is always retired after refusal.

`LIMBO_DIS64_TYPED=1` emits private JSON records with exact classic descriptor
ordinals, old/native extents, maps/leaves, complete argument/global geometry
and code. Channel/module references and by-value aggregates retain their full
argument widths. JSON symbol names are escaped. This is trusted compiler IR
lowering, not a foreign image validator or a complete module writer.

## Execution and independent checks

- AMD64 UBSan and ARM64 UBSan/QEMU each pass emu/os and WORD32/WORD32-file.
  Twenty new executions per combination use actual VM move/arithmetic/conversion/
  length/branch handlers, with collection between instructions. Managed parameters
  start with the AWM-shaped ref Draw.Context/list-of-String prefix.
- Tests include nil and Unicode/NUL Strings, real two-node list arguments,
  negative/zero/positive loop bounds, backward branches, early returns and
  big/real conversions. Real cons/consp and canonical list/element Types establish
  list owners. Owning mapped FP/MP storage preserves all inputs/results; exact
  global results, String reference counts and complete heap/Type retirement pass.
- The private replay executor treats void RET as completion and restores caller
  REG without publishing temporary C replay registers into Prog. An erroneous
  test-adapter publication was caught by UBSan on both architectures and fixed
  before publication; final matrices pass. This is not a new production
  call-stack/RET/scheduler or JIT-body execution path.
- An intentional branch-target substitution must produce the intended semantic
  result failure, not a crash/timeout. The prior field-origin control and existing
  ownership checks remain. Core IR units actively remove branch ownership,
  reference-class and both wide-immediate barriers; each fails its exact assertion.
  Late-unsupported cleanup, code limits and handler refusal are also checked.
- The independent classic Dis oracle checks actual opcode/mode, every direct
  relocation, exact descriptor extents/root bits and branch identity. Corrupt
  target/root/offset/descriptor records refuse. Enabling the report preserves
  ordinary bytes; `-64`/`-64l` still refuse the generic Kernel while reporting
  identical bodies, with no experimental output file.
- Compiler and unit ASan/UBSan pass. All 376 AWM FP/MP plans still pass, and all
  sixteen ordinary startup modules match the previous compiler. 191 existing
  private managed/Sys images remain byte-identical. Identical RU/EN example
  code compiles and passes the independent relocation oracle.

Runtime handler fixtures use UBSan. The previously documented runtime-ASan
unwind issue remains separate; compiler ASan does not close it. These use
Inferno's existing VM exclusion and do not certify FlashVM concurrent ownership.
QEMU does not establish physical Android/Bionic/device ordering or performance.

## Measured AWM frontier and remaining integration

The actual sixteen-source startup closure contains 360 compiled function bodies.
`wmsrv.hide`, `wmsrv.unhide` and `sh.debug` lower completely. 357 bodies refuse:
261 function-convention/result/handler cases, 21 indirect/reserved operands,
15 typed-span refusals and 60 unsupported-opcode cases. These are compiler
results; the three AWM bodies and the complete AWM application are not executed.

The next major boundary is value-result/call binding, followed by indirect heap
payloads and remaining code/table operations. General module writing must also
install initialized MP, descriptors and imports/exports; exception and ordinary
variadic startup remain. Frame construction/layout success is not general image
admission. No ABI guard was bypassed and no working Android GUI APK is claimed.

[English tutorial](dis64-typed-bodies.md) · [Русский](dis64-typed-bodies.ru.md)
