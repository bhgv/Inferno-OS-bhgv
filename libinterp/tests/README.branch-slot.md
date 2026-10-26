# Integer conditional branches and JMP: decoded-slot preflight

BEQB/BNEB/BLTB/BLEB/BGTB/BGEB, their WORD/LONG counterparts, and JMP now validate
used decoded slots in C, AMD64 and ARM64. Both comparison sources fit before
either load. False predicates do not validate or read the target slot; taken
predicates bound the native pointer-sized target before loading/publishing PC.
JMP validates only its target and ignores both unused source fields.

BYTE comparisons remain unsigned; WORD/LONG comparisons remain signed. Native
pointer width is independent of the WORD model. Unaligned input/target cells and
source/PC target aliases retain their existing meaning. Target instruction values
are not reinterpreted or newly constrained; these guards validate slot arithmetic,
not allocation ownership or arbitrary instruction-pointer validity.

A malformed used slot raises bounds in C or causes write-free native decline.
AMD64 inverse displacement is patched across the whole guarded taken path. ARM64
now likewise computes that displacement from actual emitted length instead of
using the former fixed three-instruction skip. No native ABI, opcode availability
or private emitter capacity changes.

Independent registered checkpoints:

* branch-slot-native.py: 9,392 C slot/error/PC/nonaccess cases across WORD32/64,
  active with NDEBUG and passing ASan/UBSan. False invalid targets and unused JMP
  source identities stay unread, and errors preserve the complete image/REG.
* branch-slot-jit.py: 160,736 AMD64/ARM64 instruction cases under Python -O.
  Independent signed/unsigned predicate and image oracles cover all relations,
  signed boundaries, byte alignments, target aliases (including PC itself),
  forbidden metadata reads, exact store boundaries, FP state, all insufficient
  code capacities, exact sizing/output and trailing canaries.
* BRANCH_SLOT_SOURCE / BRANCH_SLOT_EMITTER_DIR select explicit baseline sources;
  BRANCH_SLOT_OP selects one of nineteen commands. All 57 old command/backend
  probes reproduce an invalid used source/target read.
* The existing integer-branches C oracle retains all 3,115,024 successful cases
  and now uses explicit predicates that stay active under NDEBUG.

The stack-only C fixture disables unsupported LeakSanitizer process enumeration,
retaining address/undefined checks. Its existing setjmp/no-return warning is not
an address finding. AMD64/ARM64 availability stays 164/173 active opcodes, with
partial operand coverage. Physical ARM64 and full Inferno boot are separate checks.

## По-русски

Проверки добавлены в 18 целочисленных условных переходов и JMP. Оба источника
проверяются до чтений. Ложный предикат не обращается к цели, истинный проверяет
слот до чтения и публикации PC. JMP проверяет только цель и игнорирует источники.
Сохраняются беззнаковый BYTE, знаковые WORD/LONG, выравнивания и алиасы целей.

В ARM64 смещение через защищённую ветвь вычисляется по её фактической длине.
Проверено 9 392 случая C с NDEBUG/ASan/UBSan и 160 736 случаев эмуляции JIT;
57 запусков старой версии воспроизводят ошибку. Старые 3 115 024 проверки C
сохранены и активны при NDEBUG. Проверяется арифметика слота, а не принадлежность
произвольного адреса выделенной памяти. ABI и доступность команд не менялись.
