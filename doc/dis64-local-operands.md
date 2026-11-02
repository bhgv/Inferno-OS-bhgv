# Private typed operands from raw Dis64 code: checkpoint0077

Status: actual compact scalar-code parsing and numeric read/write facts under
TRUSTED immutable local declarations. This is not complete program admission,
file CALL activation, a new format/profile ID or a new serialized local table.

## Concrete scalar helper repair

`dis64codeslot` now rejects widths other than4/8 before modulo, including width0
in immediate mode, and rejects positive memory offsets above UINT32_MAX before
converting `long` to `unsigned int`. On LP64, an offset such as0x100000040 could
previously wrap to byte64 in a directly supplied instruction. The compact wire
reader already bounds decoded values to signed30; this fixes the helper/direct
instruction API rather than claiming ordinary encoded files contain such values.
Accepted43-op file semantics and ordinary Dis LEA are unchanged.

## Derived facts

`include/dis64-local-operands.h` reparses complete raw code payloads using the
existing direct compact operand decoder. It derives exact source/LHS read IDs,
destination write ID, scalar kind, opcode and private control-edge/target facts.
All encoded instructions are visited, including those after IEXIT. Unknown,
indirect, MP and unsupported callable/reference roles refuse in this private
adapter. The existing loader still has its own unchanged scalar admission path.

Declarations are sorted disjoint AFP spans, bounded128, with exact Int/Bool4 or
Big/Real8 kinds. Every span excludes the64-byte header and the final eight-byte
caller result tail. Alignment, whole spans, overlap, holes, overflow and exact
operand starts are checked. Equal widths do not permit bool/int or big/real
substitution. MOVW to a bool slot accepts bool slots or literal0/1 only; bool
comparisons can use exact bool operands and0/1. Bool arithmetic writes stay
outside this first typed subset. MP initializer/type proofs remain separate.

AXNON arithmetic reads the OLD destination before writing it; facts preserve
that LHS read even when source/destination alias. Big shifts read an Int4 count
and a Big8 LHS/destination. Source and middle IDs are separate optional roles,
not a packed read array: zero means immediate/unused; `reads` counts nonzero
roles, including two reads of one aliased slot. Consumers must check all reads
before publishing a write's initialized fact.

Direct instruction input is constrained to signed30 and canonical zero unused
fields. Control targets must fit the caller-selected function interval. The
whole-payload adapter currently uses one common frame/local context, not a mixed
callable-function catalog. It does not map IEXIT/IRET policy for future calls;
RET/FRAME/CALL/LEA still refuse through the existing43-op role whitelist.

## Publication and resources

The code count is bounded65536. No heap or recursion is used. The caller provides
disjoint accessible immutable bytes/declarations, disposable scratch and final
fact storage, plus a written count. Every role and exact final byte extent is
checked before copying facts or publishing count. Refusal leaves all output
facts/count/guards unchanged; scratch may contain incomplete untrusted facts.
At the maximum, two32-byte fact arrays use4MiB on the tested unsigned32 targets.
Work is bounded by instructions times at most128 declared spans and14 code bytes
per instruction; these are private implementation caps, not new wire limits.

## Independent evidence

The Python oracle implements its own compact byte encoder/parser, declaration
interval checks and opcode role/type model. It never calls the production code
or span helpers. Its17544 payload/layout cases yield707 accepted and16837 refused.
Cases cover all256 mode bytes for the scalar vocabulary and selected forbidden
operations, exact facts for supported roles, signed7/14/30 and long encodings,
same-width kind mistakes, bool boundaries, implicit destination reads, mixed-
width shifts, malformed unreachable instructions, every representative prefix
truncation, trailing bytes,128/129 declarations and65536/65537 instructions.

C probes separately check invalid widths, LP64 offsets above4GiB in FP/MP and
whole-instruction helpers, direct function-interval refusal and canonical unused
fields. Twelve active removal controls weaken narrowing/width guards, tail,
overlap, exact kind, bool range, implicit reads, shift type, all-instruction
validation, transactional output, capacity and one-record-overrun publication. Each must fail semantic status10,
not a compiler/signal/sanitizer error. Width controls encounter a safe immediate
predicate before any possible invalid modulo. Local optimized nonrecovering
UBSan passes normal cases and all controls. Scratch and final output use DIFFERENT
poison bytes; the publication control copies one extra accessible sentinel record
and must fail semantically, so equal initial bytes cannot hide an overrun.

## Remaining composition

These facts do NOT prove a read slot is initialized. They do not yet propagate
numeric initialized sets through CFG or justify callee bodies. Declaration kinds
are still immutable trusted context inputs: derive/prove them against canonical
catalog/compiler constraints before treating arbitrary files as safe. No local
metadata section or IDs are proposed by this header. Current flat scalar files
need not reserve this future callable-subset tail and are not silently forced
through this stricter private layout policy.

Next join numeric facts with entry-seeded CFG, checking every read before its
write, and compose actual callable intervals/signatures/return guarantees. Then
shared whole-file C/RX positive/refusal goldens must precede activation. No loader
or compiler source includes this private adapter.
