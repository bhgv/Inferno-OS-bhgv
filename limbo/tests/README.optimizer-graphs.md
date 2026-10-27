# Bounded optimizer graph walks and wide use/def counts

The optimizer used recursive successor DFS and recursive predecessor marking.
A long function graph could consume a C frame for each block and exhaust the
compiler thread stack. Both operations now share an explicit work stack with
64 local entries; larger traversals grow a private heap buffer with bounded
byte arithmetic. All private buffers are freed after the traversal.

The stack retains the current block and next edge, preserving discovery order,
postorder numbering, cycles, duplicate edges, unreachable nodes and previously
marked barriers. `dfs0` and `loop0` keep their existing interfaces. Other compiler
recursion, including parser/type processing, is outside this change.

Bitmap row counts, aggregate use/def counts and their diagnostic variables now
use `uvlong`. Signed `int` accumulation could overflow even when each individual
row and frame remained valid. Diagnostic formatting uses the matching wide
unsigned format. Bit cells, declaration indices, bytecode and frame layouts do
not change; this does not solve the native compiler/reference/frame ABI.

## Verification

```sh
python3 limbo/tests/optimizer-graphs.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 limbo/tests/optimizer-bits.py
python3 utils/tests/native-bootstrap.py
```

The graph driver defaults to ASan/UBSan with NDEBUG. It borrows actual private
Block/Blist/Bits declarations and production walk/count bodies. Its 1,582,284
small cases enumerate every directed adjacency matrix through four nodes,
both traversal directions, every root and three initial marking profiles.
An independent recursive oracle is used only for those tiny graphs. Full flags,
postorder numbers, remaining counters and work-buffer ownership are compared.

Separate cases execute each walk on a 30,000-node cyclic chain with a 256 KiB
C-stack limit. All nodes must be visited and the forward postorder must remain
exact. The aggregate test counts `4097*32768*32 = 4296015872` set positions using
read-only aliased row storage, requiring only a small bitmap rather than a
multi-gigabyte fixture. A volatile observation forces the old production count
to execute before its wide-result predicate; it cannot be optimized away.
The row counter's own multi-gigabit boundary is not allocated in this matrix.

The existing bitmap suite passed 16,415,642 checks in each host-ulong32/64 model
with explicit predicates active under NDEBUG. The bootstrap built actual ELF64
mk/iyacc/decgen, regenerated the real grammar and compiled all 17 Limbo units.
The verified host is AMD64; this is not an ARM64 hardware or full VM boot test.

The preceding compiler agrees with the exhaustive small-graph oracle but fails
both deep walks with SIGSEGV under the same stack limit. Its aggregate counter
independently fails UBSan on signed addition overflow. Replay with
`OPTIM_GRAPH_SOURCE` and `OPTIM_GRAPH_CASE=small|dfs|loop|count` (use the matching
compiler source and UBSan for parent controls). These are three independent
production failures, not fixture compilation failures.

Priority JIT opcode availability remains 173/173 complete entries including C
and 166/173 guarded native fast paths (95.95%) on AMD64/ARM64. This series changes
the Limbo compiler feeding the VM. No RV64-specific, MIPS/SPARC/PowerPC JIT, Tk
or security/crypto changes are included.
