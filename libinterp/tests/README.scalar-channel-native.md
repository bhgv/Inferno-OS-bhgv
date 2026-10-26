# Scalar channel construction on AMD64 and ARM64

NEWCB, NEWCW, NEWCL, NEWCF and NEWCP now have guarded native paths on both
backends. Availability increases from 155 to 160 of 173 active opcode IDs;
13 remain interpreter-only. This counts partial native paths, not complete
operand coverage or full Inferno acceptance.

The leaf validates the output cell, requires its old owner to be H, captures the
complete WORD capacity and accepts values from zero through INT_MAX. An exact
R.m == R.d alias means an unbuffered channel and does not read a capacity from
the pointer cell. R.s is unused and never accessed. Unsupported/invalid operands
return to C without allocation or stores, preserving ordinary VM error ordering.

The new cnewcscalar runtime entry selects the builtin descriptor and mover and
calls the existing cnewc allocator. Queue creation, optional array allocation,
Type reservations, initialization and protected failure cleanup remain shared
with the interpreter. Native code keeps the output address on an aligned stack,
then publishes the returned channel pointer. AMD64 uses 24 stack bytes and ARM64
uses 16, including saved LR. Existing channel replacement stays in C, where the
old owner can be destroyed safely. No separate scheduler or channel allocator is
introduced.

The interpreter's shared channelargs preflight now rejects nil/sentinel/wrapping
capacity and destination cells, nil owners, invalid heap-prefix geometry and
dead owners before resource allocation. Raw NEWCM validates this before creating
a temporary Type. NEWCM and NEWCMP also validate their source-word spans before
reading widths/indices. Descriptor and buffer overflow checks remain in cnewc.
These checks establish arithmetic/lifetime contracts; they cannot prove arbitrary
native addresses are mapped.

Run from the repository root:

```
python3 libinterp/tests/scalar-channel-native.py
python3 -m pip install -r libinterp/tests/requirements-jit.txt
python3 libinterp/tests/scalar-channel-jit.py
CHANNEL_NO_ERRORS=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/scalar-channel-native.py
```

Evidence:

- The production C/actual AMD64 RX driver passes 80,640 allocation, capacity,
  alias, replacement, descriptor, cleanup and FP-state cases for each of WORD32
  with native pointers and legacy WORD64. Allocation cutoffs sweep every queue
  and buffer stage. Live resources and builtin Type references balance after
  every case; failures preserve the old output and complete operand image.
- Fifty preflight cases per WORD model cover scalar, raw and mapped channel
  slots, nil heap prefixes, dead owners and no-allocation behavior. Replacing
  compatible argument-consuming functions with parent versions through
  CHANNEL_ARGUMENT_SOURCE reproduces the parent invalid-read failure under UBSan.
- The machine-code driver passes 81,920 cases per architecture. The high-address
  callback checks opcode/capacity and delayed publication, then clobbers ordinary
  caller-saved registers. Tests check native stack alignment, nonvolatile registers,
  restored LR, full VM images, bounded stores and FP control/status cells. Simulated
  allocation errors stop at helper entry; actual AMD64 error unwind is tested by
  the C/RX driver.
- Successful subsets pass ASan+UBSan: 5,760 cases per WORD model. Explicit resource
  counters remain active while unsupported LeakSanitizer process inspection is
  disabled by the fixture. Complete error sweeps use UBSan and NDEBUG.
- The core checkpoint passes 116 host drivers. Existing channel ownership,
  descriptor reservation, raw-width alias and slot-alignment drivers also pass.
  The complete xec.c and both backend translation units pass host syntax checks.

Physical ARM64 execution, complete compiler/VM builds, concurrent scheduling/GC
and platform-specific native acceptance remain separate. VM and Dis layouts and
MIPS/SPARC/PowerPC JIT sources are unchanged.

Raw/mapped constructors and pre-publication raw metadata now share the same
capture/publication protocol; see [README.payload-channel-native.md](README.payload-channel-native.md).
The common allocator now preflights maps for unbuffered mapped channels as well
as buffered descriptors. The historical counts above describe the scalar series.
