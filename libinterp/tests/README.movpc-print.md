# Native Dis PC addresses and format/result slots

The x86-64, arm64 and RV64 decoded-operation backends now compile MOVPC.
Private layout adapters provide REG.M, Modlink.m, Module.prog, Module.nprog and
sizeof(Inst); no public register, module, frame or Dis wire layout is changed.
The leaf validates both owner identities, the signed instruction count, the full
WORD index and the backing program identity before publishing a native pointer.
A native address carry declines without output. RV64 requires multiplication
(M or Zmmul), and does not require division. WORD32 host models still publish a
complete native pointer, exactly like the C MOVPC handler.

The C handler also rejects H program backing and address wrap before numeric
pointer construction; it no longer forms an out-of-range C array pointer for
malformed program metadata. Ordinary instructions keep their original PCs.
This is address materialization, not executable-code relocation.

The shared formatter now reads String*, REAL, LONG and debug heap-pointer
arguments with byte-safe copies. It validates nil formats/output buffers/string
arguments and INT_MIN representation lengths before dereferencing or negating
sizes. Literal narrow bytes are promoted unsigned, consistently with Rune
promotion. Existing Dis argument steps and frame-relative alignment are retained;
this change does not redefine variadic argument packing.

Formatted results keep the returned byte count when constructing a VM string.
Sys.sprint therefore preserves an embedded NUL and all subsequent characters.
Sys.sprint and Sys.aprint publish owners through alignment-safe return slots,
retaining their existing conversion-failure cleanup behavior.

## Checks

```
PYTHONPATH=../jit-test-deps LEAF64_CASE=movpc python libinterp/tests/leaf64-jit.py
MOVPC_NATIVE=1 python libinterp/tests/vm-addresses.py
SYS_PRINT_CASE=alignment python libinterp/tests/sys-print.py
SYS_PRINT_CASE=invalid python libinterp/tests/sys-print.py
SYS_PRINT_CASE=embedded python libinterp/tests/sys-print.py
python libinterp/tests/sys-print.py
python libinterp/tests/slice-tokenize-suite.py --core
```

Use `CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'`
for the native address and selected formatter families. Fixtures extract real
production bodies. WORD32 models retain host pointers. The formatter fixture
links and installs the real lib9 floating formatter for REAL-argument coverage;
it does not substitute host printf for production formatting.

* Three-backend MOVPC emission/execution: 18,832 checks. Capstone decodes and
  Unicorn executes both word models, owner/bounds/backing/overflow combinations,
  native pointer-width results, exact/partial result aliases and rejection before
  inaccessible operands/results. Metadata and REG owner slots must remain intact.
* Actual C/native AMD64 addresses: 3,536 checks across both word models,
  including 11 independently protected rejection families, all source/result
  byte alignments and high native pointers. These pass ASan/UBSan.
* Formatter alignment/unsigned literal promotion: 768 checks per word model.
  REAL/LONG frame-relative alignment is independent of physical native alignment;
  String/debug pointers and both narrow/Rune formats are covered.
* Malformed formats/owners/buffers: 56 cases per word model, preserving output
  bytes and balancing the actual emulator error stack.
* Embedded-NUL results and unaligned owner slots: 32 cases per word model,
  exercising Sys.sprint/Sys.aprint with both string representations.
* These three new formatter families (1,712 total cases) pass ASan/UBSan;
  the complete formatter driver passes UBSan with 1,014 cases per word model.

Parent overrides `VM_ADDRESS_SOURCE` and `SYS_PRINT_SOURCE` reproduce the H
program-pointer fault, unaligned argument loads, null-format dereference and
embedded-NUL truncation. Native AMD64 lifetime/runtime regression passes both
word models. ARM64/RV64 publication and emulated execution do not constitute a
hardware boot or complete native VM acceptance test.
