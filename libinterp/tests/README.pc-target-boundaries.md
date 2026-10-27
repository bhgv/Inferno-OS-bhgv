# Complete instruction spans for table targets and MOVPC

CASE, CASEL, CASEC, GOTO and MOVPC previously checked addition of the program
base and instruction index, but not the complete `Inst` span at the resulting
address. An addition could succeed while producing H or a target too close to
the end of the native address space to hold an instruction.

The C handlers now bound both the multiplied offset and the final instruction
extent. AMD64 and ARM64 numeric/string case kernels and program/table address
kernels check the same extent before publishing PC or the MOVPC output slot.
No frame or reference ownership changes are introduced. Legacy relocated
native-PC case/goto tables retain their existing C path and representation.
MOVPC continues to address original Dis instructions.

These are numeric span checks. They do not prove that an address belongs to an
executable allocation. The boundary fixtures construct high-address PCs without
fetching or executing synthetic target code.

## Validation

Run from the repository root; guest tests require Unicorn:

```sh
python3 libinterp/tests/pc-target-boundary.py
python3 libinterp/tests/pc-tables-native.py
python3 libinterp/tests/string-case-native.py
python3 libinterp/tests/goto-native.py
python3 libinterp/tests/pc-tables-jit.py
PC_TARGET_STRIDE=1 python3 libinterp/tests/pc-tables-jit.py
PC_TARGET_STRIDE=2047 python3 libinterp/tests/pc-tables-jit.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The new native driver borrows all five actual C operations, the real compiler
and cjitexec. It runs direct C, fast-plus-C and complete RX entries with native
pointers, WORD32/64 and independent four/eight-byte wire cells. Descriptor and
operand bytes remain real native objects. The instruction body is not decoded
at the fabricated address. CASEC boundary cases use empty tables; nonempty string
comparisons remain covered by the separate real string-case fixture.

| Matrix | Result |
| --- | --- |
| Native full-Inst boundary routes | 90,720 total: 10,080 accepted and 80,640 rejected |
| Existing native PC table C/RX matrix | 11,559 cases per WORD/cell profile, four profiles |
| Existing native string case C/RX matrix | 2,138 cases per WORD/cell profile, four profiles |
| Numeric case wire/alignment/relocation matrix | 614,428 cases per WORD model |
| Large case native/fallback boundaries | 64 cases per WORD model |
| Native GOTO table/alias/legacy cases | 88,733 per WORD model |
| Guest tables, instruction stride 24 | 58,112 total across both architectures |
| Guest tables, instruction stride 1 | 55,232 total across both architectures |
| Guest tables, instruction stride 2047 | 58,112 total across both architectures |

The native boundary matrix exhausts the final `sizeof(Inst)+3` address positions,
five destination indices and all operand/table byte shifts. Numeric CASE/CASEL
exercise both found and default destinations. Every invalid route preserves the
complete REG and data images and raises exBounds. ASan/UBSan and NDEBUG remain
enabled; each C/JIT error-route topology runs in a separate process.

Guest matrices independently encode metadata in compact and distant layouts.
They check the exact final accepted instruction span, invalid complete spans,
nil/H/wrapping metadata, wire widths, all five operations, preserved host stack,
callee-saved registers and FP controls. Refusal must have an empty VM write
trace, including same-value stores. Sizing-only, exact capacity, insufficient
capacity and output-suffix checks verify atomic code publication. For stride 1,
a program near the address limit can still hold a complete instruction; the
oracle computes the full target extent rather than treating every high address
as invalid.

Two older native fixtures now initialize their unused instruction slots to
IRUNT. Leaving those slots as zero made the stricter compiler reject the whole
fixture as an INOP-containing program before table execution. Production opcode
validation is unchanged.

Twenty independent parent-381ecd2 probes reproduce the missing extent check:
each of five C operations under both WORD models, and each of five emitted
operations on AMD64 and ARM64. Native probes accept PC_TARGET_SOURCE,
PC_TARGET_FAMILY=0..4, PC_TARGET_ROUTE=0, PC_TARGET_WORD_BITS and PC_TARGET_CELLS.
Guest probes accept PC_TARGET_EMITTER_DIR, PC_TARGET_GUEST_OP=0..4,
PC_TARGET_ARCHES and PC_TARGET_BOUNDARY_ONLY=1. The family order is CASE, CASEL,
GOTO, MOVPC, CASEC.

Native runs in this series were AMD64. ARM64 machine code was tested in Unicorn;
a physical ARM64 run remains outstanding. Complete AMD64 dispatch also retains
177,152 routes, 5,002 compiler and 295 integration checks per WORD model; ARM64
compiler preflight retains 5,002 per model.

## Port completion is separate from opcode coverage

Both priority backends retain 173/173 complete JIT entries including C and
166/173 guarded native fast paths. These counts do not measure port completion
or full operand-domain coverage.

The production-header probe was rerun:

```sh
python3 utils/tests/native-vm-abi.py --require-compatible
```

It exits with status 1 on this AMD64 host:

| Contract | Current measurement | Status |
| --- | --- | --- |
| Runtime WORD versus compiler cell | 8 versus 4 bytes | incompatible |
| Native pointer versus compiler reference cell | 8 versus 4 bytes | incompatible |
| Native Frame versus compiler temporary start | 32-byte header versus byte 20 | overlap |
| Full native VM/Limbo-program boot | not demonstrated | outstanding |
| Physical ARM64 runtime acceptance | not run | outstanding |

Compiler reference layout, frame offsets, loader data and GC maps still require
a coordinated target ABI; see
[README.native-bootstrap.md](../../utils/tests/README.native-bootstrap.md).
The scope of this commit is instruction-target correctness and its regression
coverage. RV64, Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources are unchanged.
