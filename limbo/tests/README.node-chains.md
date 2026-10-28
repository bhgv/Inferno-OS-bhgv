# Bounded compiler stacks for declaration and label chains

Run `python3 limbo/tests/node-chains.py` from the repository root. The fixture
extracts the real `mkn`, `mkdeclname`, `mkbin`, `varinit`, `caselist` and `occurs`
bodies and uses the compiler's actual Node, Decl, Type and Src declarations.
`CC`, `CFLAGS`, `NODE_CHAIN_SOURCE` and `NODE_CHAIN_CASE=small|varinit|caselist|occurs`
select the compiler, flags, source and family.

Three production changes remove depth-proportional C stacks on common chains:

- `varinit` first builds private name nodes, then constructs assignments from
  the last declaration back to the first. Temporary name links are cleared.
  This retains the original allocation order, exactly two nodes per declaration,
  declaration reference increments and right-associated assignment order. The
  input RHS appears exactly once at the end of the chain. No private work buffer
  or additional allocation is introduced.
- `caselist` reverses right links with a loop, retaining the existing suffix,
  order and every other node byte. This serves case/alt/pick and exception labels.
  It no longer relies on the host compiler removing tail recursion.
- `occurs` accumulates left-subtree counts while iterating along right links.
  Oname remains terminal even if its child fields are populated; nil still has
  zero uses. Recursion into left subtrees remains, so this is not a universal
  bound for arbitrarily deep ASTs.

The nonempty declaration/label preconditions are unchanged. Grammar, bytecode,
frame/reference widths and native JIT ABIs are unchanged. Other recursive
compiler/parser passes remain outside this change; a successful local deep
transformation is not a claim that an arbitrary 100,000-label source compiles
through the entire pipeline.

The ordinary oracle covers 29,184 structural cases: declarations through 512
names with sixteen RHS/source/type patterns, labels through 512 entries with
five suffix lengths and eight metadata patterns, and independently constructed
reference-counting trees through 512 entries. It checks AST association, ordered
names, exact declaration-reference changes, source spans, a single shared RHS,
unchanged input bytes, allocation count and guards. Label results are compared
with a direct full-node permutation. Reference inputs are built independently
of either production chain constructor and test both present and absent names.

Three isolated deep families each execute 100,000 entries with a 256 KiB C-stack
limit. They verify results iteratively, without a recursive test oracle.
The default flags are O1, ASan/UBSan, NDEBUG and
`-fno-optimize-sibling-calls`; explicit predicates remain active. LSan's unavailable
process inspection is disabled; address/stack instrumentation is retained.
The latter compiler flag makes the stack contract independent of a tail-call
optimization and is not a production build requirement.

Complete runs pass in three profiles: O0 with ASan/UBSan, O1 with ASan/UBSan and
sibling-call optimization disabled, and O2 with UBSan and sibling-call optimization
disabled. That is 87,552 ordinary cases and nine deep executions. The original
`56e3f67` sources pass ordinary AST oracles but reproduce nine independent stack
failures: all three transformations in O0 and O1, varinit/occurs in O2, and a
separate varinit run with ordinary O1 options. O2 already converts the old
caselist recursion into iteration on this host; that compatibility run passes
and is not counted as a reproduced failure. Compile failures never count as
regression evidence.

The native bootstrap also builds ELF64 mk/iyacc/decgen, regenerates the real
parser/decoder and compiles all 17 Limbo C units. Adjacent identity-folding,
optimizer-range and complete JIT-dispatch tests provide regression coverage.
The verified host is AMD64; ARM64 hardware, full interpreter boot and coordinated
64-bit compiler/loader/frame/GC layouts require separate acceptance tests.
Priority opcode coverage remains 173/173 JIT+C entries and 166/173 guarded native
paths for each of AMD64 and ARM64.

## Both associations in occurs / Оба направления occurs

The follow-up removes the remaining left-child recursion from declaration-use
counting. A fixed 64-pointer work area handles small trees; checked geometric heap
growth handles larger pending branches. Nodes and declaration references are never
mutated. Oname stays terminal even when its child fields contain cyclic metadata;
DAG edges retain their ordinary per-edge occurrence meaning. Temporary work memory
is freed on success. Allocation/size exhaustion or signed count overflow is fatal,
as with other internal compiler resource failures; this is not recoverable API state.

The `occurs-left` family mirrors 100000 right-associated nodes under the same
256 KiB C-stack limit. It reproduces ASan stack overflow on 3523d0d before the fix.
Small structural checks cover both directions, while existing varinit/caselist
and right-chain cases remain unchanged. This bounds this counter's C stack, not
every compiler/optimizer traversal or total AST work.

Дополнительное исправление убирает оставшуюся рекурсию по левому потомку при
подсчёте использований объявления. Малые деревья используют 64 указателя на
фиксированном C-стеке, большие — проверенное геометрическое расширение в куче.
Узлы и счётчики ссылок объявлений не меняются. Oname остаётся терминальным даже
с циклическими служебными полями; общие ветви DAG считаются по каждому ребру.
Временная память освобождается при успехе; нехватка памяти, переполнение размера
или signed-счётчика — fatal внутреннего компилятора, без обещания восстановления.

Семейство `occurs-left` зеркалит 100000 узлов при лимите C-стека 256 КиБ.
На 3523d0d до исправления воспроизводится stack overflow ASan. Малые структурные
проверки охватывают оба направления, прежние varinit/caselist/right-chain сохранены.
Это ограничение стека именно occurs, а не всех обходов компилятора/оптимизатора.
