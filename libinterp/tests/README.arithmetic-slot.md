# Division and real-branch decoded-slot preflight

Upstream 2190eea DIVB/MODB/DIVW/MODW/DIVL/MODL bound the complete divisor slot before its
load in C and AMD64/ARM64 leaves. A zero divisor is diagnosed before dividend or
result access. For WORD/LONG C this deliberately aligns error priority with the
existing native leaves; ordinary valid division and MIN/-1 results are unchanged.
With a nonzero divisor both remaining slots fit before the dividend is read.
All scalar operands are captured before a possibly overlapping result store.

BEQF/BNEF/BLTF/BLEF/BGTF/BGEF preflight both REAL source slots before either
load. A false predicate neither validates nor reads its target slot. A taken
predicate bounds the native target slot before reading it or changing PC. The
ARM64 inverse branch is relocated over the complete guarded taken path, rather
than assuming its old three-instruction length. Native NaN decline, interpreter
NaN behavior and host FP control/sticky semantics remain unchanged.

Malformed used slots raise bounds in C; zero division raises the existing zero
error. A declined leaf preserves bytes, REG and FP state. Pointer width is native,
independent of WORD32/WORD64; arithmetic bounds cannot prove arbitrary mapped
allocation ownership. No private capacity, opcode or native ABI change occurs.

Registered independent checkpoints:

* arithmetic-slot-native.py: 5,472 cases across WORD32/WORD64. NDEBUG and
  ASan/UBSan checks cover malformed spans, zero-divisor priority, forbidden early
  reads, taken/untaken targets, PC publication and error atomicity.
* arithmetic-slot-jit.py: 67,584 AMD64/ARM64 instruction cases across both WORD
  models. Independent integer truncation/remainder and predicate/image oracles
  check signed extrema, MIN/-1, full/partial aliases, all byte alignments, zeros,
  NaN decline, infinities, invalid slots, false-target nonaccess, REG/PC bytes,
  FP state, every short output size, exact-capacity image and trailing canary.
* ARITH_SLOT_SOURCE / ARITH_SLOT_EMITTER_DIR select explicit previous-version
  inputs; ARITH_SLOT_OP selects one of twelve commands. All 36 old-version
  command/backend probes reproduce the original null source-slot fault under -O.
* Existing shift/division and real-branch differential matrices remain active;
  minimal real fixtures now declare the bounds identity and valid-only error stub.

The stack-only C fixture disables unsupported sandbox LeakSanitizer enumeration,
retaining AddressSanitizer and UBSan. Its existing setjmp/no-return warning is
recorded; this fixture provides no leak-detection evidence. Native availability
remains AMD64/ARM64 164/173 with partial operand coverage. Physical ARM64 execution
and full Inferno boot remain separate checks.

## По-русски

Исправлены шесть делений/остатков и шесть вещественных переходов C/AMD64/ARM64.
Нулевой делитель проверяется раньше обращения к делимому и результату. Для C
WORD/LONG это намеренно согласует приоритет ошибок с уже существующим JIT.
При допустимом делителе оба оставшихся слота проверяются до чтения делимого.
Результаты арифметики, включая MIN/-1, сохраняются.

У переходов проверяются оба источника до чтений. Ложный предикат не обращается
к слоту цели; истинный проверяет его до чтения и публикации PC. В ARM64 смещение
обратного перехода вычисляется по фактической длине защищённой ветви.

Независимые проверки: 5 472 случая C с NDEBUG/ASan/UBSan, 67 584 случая эмуляции
JIT, 36 воспроизведений ошибки на старом коде. Контролируются перекрытия, все
выравнивания, ноль, знаковые границы, NaN/бесконечности, FP-состояние, память,
неиспользуемые цели и границы буфера кода. Проверка диапазона адреса не доказывает
принадлежность выделенной памяти. ABI и доступность команд не менялись.

The concurrent 2190eea integer series supplies the production division guards;
those broader incoming implementations are preserved. This commit adds the six
real-branch guards and independent twelve-command tests above. The composite
native fixture supplies always-active transfercheck and integercheck macros for
standalone oracle bodies whose original helper prefixes are not embedded.
