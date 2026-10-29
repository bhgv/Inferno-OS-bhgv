# Guarded RET leaves and validated stack teardown

AMD64 and ARM64 implement RET for ordinary pointer-free frames, including saved
same-mode module owners whose release leaves a live reference. See
[Native module returns](README.module-returns.md) for the current contract and
validation matrices. The saved FP/PC/MP and owner counts are captured before VM
publication. No-saved-module returns preserve ordinary behavior without reading
the current module header. The 64-unit staging bound remains.

Mapped frames, stack extensions, root returns, last-reference module release and
compiled-mode transitions use C fallback. Every declined leaf is write-free.
C RET validates nil/H frame links, return PCs, Type owners/map lengths and module
identities before publication. A saved-module return also rejects a dead current
heap owner, or a self-module return with only one counted owner, before frame
cleanup. The empty-message root-return exception retains
priority. Extension Type metadata is captured from Stkext before REG changes.

unextend independently rejects nil/H frames and nil/H/negative-map extent Types
before restoring SP/TS/EX or freeing storage. Existing extension and module-return
cleanup order is preserved. Stack lifecycle, return-slot, exception and startup
fixtures cover these separate fallback and error paths.

Current availability among 173 active commands (partial guarded native paths):

| Backend | Native paths | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 139 | 34 |
| ARM64 | 139 | 34 |
| RISC-V64 M | 103 | 70 |
| RISC-V64 Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

INOP and unused IECLR are excluded. Complete operand coverage and the complete
native VM ABI remain separate work. ARM64 is tested through emulation.

See [stack cleanup across REG borrowing](README.stack-callbacks.md) for private
unwind cursors, resumed RET/extension state and destructor regression matrices.
