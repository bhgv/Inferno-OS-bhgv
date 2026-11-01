# Independent RV64 complete-tail decoder

`riscv-bridge-oracle.py` compiles the actual owning `riscv-emit.h` and interprets
its output using an independent bounded RV64I instruction decoder. It checks
register and literal behavior, rather than comparing another copy of expected
instruction words. The decoder supports only the current bridge instruction
subset; it is not a general VM, a target CPU or pointer authentication.

```sh
python3 libinterp/tests/riscv-bridge-oracle.py
CFLAGS='-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' \
    python3 libinterp/tests/riscv-bridge-oracle.py
```

Generated files stay in ordinary temporary scratch; `TMPDIR` may select an
existing writable directory outside ROOT. The default gate uses O3/NDEBUG strict
UBSan. Expected results remain active without assert.

Every active opcode runs every permitted pointer-bit position with nil/non-nil
fast targets, mixed low/high-word edge patterns and capacities-2..16. Sizing and
emission must agree, short buffers must remain completely untouched, and bytes
outside actual output retain canaries. Successful streams are interpreted at
both eight-byte and four-byte entry alignments above4GiB. The decoder reconstructs
LWU/LW, shifts, OR, ADDI, AUIPC and JALR by architectural fields; AUIPC uses its
own PC, and every literal read is bounds/alignment checked.

447,032 valid-input transactions check the exact executor, opcode and optional
fast pointer. Incoming a0, ra, sp, gp, tp and callee-saved registers must survive
at the tail boundary. Ordinary other caller-saved registers may change. Separate
refusal cases cover all inactive/out-of-range opcodes, nil/odd/H pointer sentinels
and malformed pointers at every capacity-1..64 without output mutation.

Three separately compiled controls must fail at the active decoder predicate:
change LWU to sign-extending LW, change tail JALR to write ra, or replace a1 with
a0 when loading the opcode. A sanitizer crash alone is insufficient evidence.
These controls independently exercise high-bit reconstruction, disposable-code
return lifetime and incoming REG preservation.

This is host-side encoder/ABI metadata evidence. Real cjitexec, compiler cache
publication, target instruction execution and code-page retirement retain their
separate owning tests. In particular, this oracle does not replace the six
Linux/QEMU target configurations from [the complete-tail guide](README.riscv-dispatch.md).
It assigns no Dis64 wire metadata, feature bits or calling signature.
