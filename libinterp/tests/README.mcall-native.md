# Same-mode MCALL and preflighted module ownership

AMD64 and ARM64 now implement MCALL for nonnegative bytecode import indices
when the target and caller compiled flags match. Builtins, execution-mode
transitions and named export ordinals use the interpreter path. The saved caller
is transferred to Frame.mr, the callee is retained and installed in REG.M, and
IC remains unchanged, following the equal-mode C rules.

The native leaves validate module/program owners, signed import counts, full
WORD indices, entry targets, frame owners and reference-counter capacity before
any VM store. Invalid module owners decline before reading the index. Import
scaling uses native addresses and explicit 16-byte entry geometry, with a private
program-entry offset derived from Modl.u. A last legal retain to ULONG_MAX is
accepted; zero or saturated owners decline without writing a partial frame.

Success captures old PC/FP and module data before replacing frame links, retains
the callee once, installs it as the current module, restores MP from the module link, publishes the entry
PC and stores WORD-sized t=1. Frame and operand aliases are captured before
writes. Both backends keep the existing 64-unit limit and preserve FP state.

The C implementation now rejects nil/H module owners and invalid current module
owners before fetching the index. It preflights the target reference before
writing frame links or changing REG, and rejects invalid bytecode program/entry
markers and nil/H runtime function entries before activation. Builtin cleanup
and cross-module IC transitions remain in their existing C path.

LeafLayout gains explicit REG MP/temp, module-link program/data and import-entry
PC offsets after stringmax. The production compiler initializes every new field.
The ctypes declaration is extended in the same commit; existing C size/offset
probes check the complete record before emission.

Validation:

- mcall-native.py: 24,576 C/published RX AMD64 cases per WORD model, covering
  indices 0/1/2, unaligned operands/frames, self/equal-mode/cross-mode/builtin calls,
  named-export fallback, refcount saturation and the last legal retain,
  invalid programs/entries/owners, exact frame/REG/slot failure images and
  callback/refcount effects. Selected ASan/UBSan runs pass.
- mcall-jit.py: 9,216 independent image/write/FP checks per backend, covering
  both WORD models, full-width references and pointers, import stride and
  entry-PC offsets 0/8,
  operands inside overwritten frame links, nil/H metadata, inaccessible index
  operands and write-free fallback. Sizing, decoding, malformed layouts and
  bounded publication are also checked.
- All 89 --core host drivers pass. Native publication/fallback/metadata/retry/
  unmap checks pass. The ctypes ABI probes and byte-branch/character selections
  pass 15,474 and 6,814 checks respectively.
- Parent-source replay fails the new C regression driver. Removing the saturated
  reference check or entry validation independently makes it fail as well.

| Backend | Guarded native opcodes | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 138 | 35 |
| ARM64 | 138 | 35 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

Counts cover partial guarded paths among 173 active instructions, excluding
INOP and unused IECLR. Complete operand coverage and complete native VM ABI
acceptance remain separate work. ARM64 is tested by instruction emulation;
physical target execution remains outstanding. Excluded architecture and
Tk/security/crypto sources are unchanged.
