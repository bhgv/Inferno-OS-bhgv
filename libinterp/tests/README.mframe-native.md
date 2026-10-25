# Imported MFRAME and invalid frame maps

AMD64 and ARM64 now implement MFRAME for nonnegative imported-entry indices,
pointer-free frame types and frames that fit strictly within the current stack
extent. The inline Modlink.links table supplies the frame Type directly; neither
the module's Type table nor the legacy Modlink.type snapshot is consulted.

The private layout records the import count, inline table offset, native entry
stride and entry-frame offset. The current native Modl layout has a 16-byte
stride. Full WORD indices are checked against a signed, nonnegative import
count before native-width address scaling. Nil/H module owners decline before
reading the index operand, matching the C error boundary even if that operand
is inaccessible. Negative export ordinals retain the C lookup path.

MFRAME shares FRAME's validated allocation tail: Type/size/map/extent checks
precede writes to SP, Frame.t, Frame.mr and the result slot. Pointer-bearing
frames and extent growth retain C fallback. The 64-unit staging limit and real
VM structures are unchanged; publication derives private offsets from C.

Two C defects are also corrected:

- framefits and stacksize reject negative Type.np before frame publication,
  stack allocation or stack mutation. Previously initmem could silently skip
  the malformed map. This covers FRAME/MFRAME, newstack, extend and unframe's
  existing stacksize validation.
- moduleentry rejects H as the named-export table before scanning Link names;
  the caller retains the ordinary invalid-entry error and unmodified output.

Validation:

- mframe-native.py: 10,368 C/published RX AMD64 cases per WORD model, testing
  indices 0/1/2 with invalid decoy entries, exact stack/REG/slot images,
  overlapping operands, map/extent fallback and invalid stack endpoints.
  Additional checks cover nil/H module owners with an inaccessible index and
  an H export table. Selected ASan/UBSan runs pass.
- mframe-jit.py: 13,824 independent image/write/FP checks per backend, for both
  WORD models. Includes signed/wide index bounds, native import stride, second
  and third entries, unused module metadata, guarded inaccessible operands,
  malformed layouts, decoding and bounded publication.
- frame-native.py now adds negative-map error atomicity (3,456 C/RX cases per
  WORD model). The existing 12,288 FRAME emulation cases per backend pass after
  sharing the allocation tail.
- stack-lifecycle.py adds negative-map rejection for newstack, extend and
  unframe before allocation/free/publication. Its invalid family has 211 checks
  per WORD model. Parent-source replay fails the added regression.
- All 85 --core host drivers and actual AMD64 compiler publication, fallback,
  metadata failure/retry and unmap checks pass.
- Removing either the C frame-map rejection or export-table guard independently
  fails the MFRAME regression driver.

| Backend | Guarded native opcodes | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 136 | 37 |
| ARM64 | 136 | 37 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

Counts describe partial guarded paths among 173 active instructions, excluding
INOP and unused IECLR. They do not imply full operand coverage or a complete
native VM ABI. ARM64 is checked by instruction emulation; physical execution
remains outstanding. Excluded architecture and Tk/security/crypto sources are
unchanged.
