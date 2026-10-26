# Checked decoded slots for integer shifts

SHLB/SHRB/SHLW/SHRW/LSRW/SHLL/SHRL/LSRL preflight the complete WORD count
slot before loading it in C, AMD64 and ARM64. Null, H and wrapping spans raise
bounds in the interpreter or decline without writes in native code. An invalid
count still raises invalid argument before value/result validation or access.
For valid counts both value and result slots are bounded before reading value.
All operands are captured before a possibly overlapping result publication.

Guards use the native pointer width independently of the selected WORD32/WORD64
model. Count widths remain WORD and value/result widths remain BYTE/WORD/LONG.
Arithmetic address checks do not establish that arbitrary nonwrapping addresses
belong to a mapped allocation. Existing shift value/count semantics, opcode
availability, private code capacity and native ABI are unchanged.

The new independent checkpoints are registered in slice-tokenize-suite.py:

* shift-slot-native.py extracts the actual C handlers. Its NDEBUG/UBSan fixture
  checks 2,944 malformed slot, error-priority and byte-atomicity cases over both
  WORD models. Invalid destinations use an unmapped value pointer to detect
  premature value reads. Invalid counts use unmapped value and result pointers.
* shift-slot-jit.py executes emitted AMD64/ARM64 instructions. Its independent
  image oracle covers every legal count, signed edge values, all byte alignments,
  malformed spans, error nonaccess, exact output/canaries, every short capacity,
  unchanged REG/PC and FP state, and write-free decline: 51,200 cases.
* SHIFT_SLOT_SOURCE and SHIFT_SLOT_EMITTER_DIR select explicit baseline sources;
  SHIFT_SLOT_OP selects one of eight command indices. All 24 old-version
  command/backend probes reproduce an invalid count-slot read under Python -O.
* Existing integer-ops and shift-div checks preserve arithmetic/overlap coverage;
  their minimal declarations now include the production bounds-error identity.

AMD64/ARM64 availability remains 164/173 active opcodes, with partial operand
coverage. Physical ARM64 execution and full Inferno boot remain separate checks.

## По-русски

Проверки слотов добавлены в восемь целочисленных сдвигов интерпретатора C и JIT
AMD64/ARM64. Сначала проверяется слот и значение счётчика. Неверный счётчик
выдаёт прежнюю ошибку до обращения к значению и результату. При допустимом
счётчике проверяются оба оставшихся слота до чтения значения. Ошибочный адрес
не должен приводить к частичному изменению памяти или REG.

Независимые проверки: 2 944 случая C с NDEBUG/UBSan и 51 200 случаев эмуляции
JIT, включая все допустимые счётчики, выравнивания и границы буфера кода.
Прежние версии воспроизводят ошибку во всех 24 проверках команды/бэкенда.
Проверка арифметики адреса не доказывает принадлежность выделенной памяти;
ABI, ёмкость эмиттеров и доступность команд не менялись.

The C checkpoint also passes AddressSanitizer with NDEBUG/UBSan. Its stack-only
fixture disables unsupported LeakSanitizer process enumeration through the
standard fixture hook; leak detection is not covered. The sandbox sanitizer
reports its existing setjmp/no-return stack warning, without an address finding.
