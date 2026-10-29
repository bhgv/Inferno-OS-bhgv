# Dis64 payloads: ответ FlashVM на INFERNO-0008

Дата 2026-10-29. База: проверенное дерево envelope
c8a76ea8fa924742bcce8e2a050ae9d273726e9e, опубликованное как
4b8cb80bf5cc745b3e2e3e0bad1c3d44a00fd4f4. Основание:
[предложение Inferno](dis64-payload-proposal.md), [общий контракт](dis64-contract.md).
Это явные решения по перечисленным пунктам и отдельные кандидаты для ответа;
полный executable ABI этим документом не включается.

## Принимаю предложенный wire без изменения

1. Module metadata — последовательные BE u32: name_string_id, profile,
   data_bytes; при data_bytes!=0 затем data_type_id; только у executable затем
   entry_pc, entry_frame_type_id. Profile0=library, profile1=executable принимаю.
   Точная длина определяется profile/data_bytes, хвост отвергается. Имя non-nil,
   ID находится в pool; политика имён/import resolution остаётся отдельной.
   Отсутствующее data_type_id не заменять неоговорённым sentinel.
2. Types — count:u32, ordinal IDs0..count-1; каждый descriptor содержит
   object_bytes:u32, alignment:u32 и полную map8 длины ceil(object_bytes/64).
   Structural descriptor не объявляет ссылочные типы совместимыми и не заменяет
   callable signature. Entry-frame descriptor ссылается через module field.
3. Strings — count:u32, затем ровно count non-nil записей rune_count:u32 +
   BE u32 Rune. Полный расход секции обязателен. Count0 допустим; одинаковые
   строки под разными IDs допустимы. Nil ID не появляется. Сравнение остаётся
   signed32 lexicographic, без normalization/scalar filtering.
4. Code framing — instruction_count:u32, затем byte opcode/address-mode и
   компактные signed30 operands; ровно instruction_count инструкций и точный
   конец секции. Это принятие framing, а не всех legacy operand roles.
5. CASE/CASEL/CASEC/GOTO table_id — nonnegative signed30 immediate вместо
   адреса mutable module data принимаю именно для нового discriminator.
   Конкретное положение и roles уточняю ниже; old Dis/NC64 не меняется.

Дальнейшие пункты — конкретные предложения, ожидающие ответа Inferno.

## Types и Frame: необходимые дополнительные проверки

Для текущего natural ABI предлагаю alignment только1/2/4/8; object_bytes
кратно alignment. Пустой descriptor: bytes0, alignment1, map длины0. Эти
ограничения не создают type-kind IDs. Значения object_bytes и count имеют
отдельные resource limits до выделения памяти; карта проверяется dis-layout.h.

Entry frame: bytes>=64, alignment8. Runtime header lr/fp/mr/t, result address
и temps не становятся heap roots только из-за ширины8. Нужна отдельно
согласованная safepoint/root модель pointer-capable temps и вызовов. Для
первого executable fixture предлагаю scalar-only frame/map0, без allocation,
C calls, channels, exceptions или ref-result. Это первый gate, не обещание
полной миграции GC/ABI. Ссылочные locals и callee/result acceptance следуют
отдельными исполняемыми fixtures.

## Точные code roles для начального whitelist

Порядок сериализации operands сохраняем существующий: encoded middle, source,
destination согласно mode byte. AMP/AFP — byte offsets, не номера cell8;
AIMM — signed30 immediate, AXXX/AXNON — отсутствие operand. Первоначально
indirect addressing предлагаю отвергать, пока не зафиксированы ref64 roles.
Неизвестные opcodes/mode combinations отвергать, а не fallback на old decoder.

| Операция | Source | Middle | Destination |
| --- | --- | --- | --- |
| MOVW/MOVL | scalar read, immediate или MP/FP | absent | MP/FP write, 4/8 bytes |
| MOVF | MP/FP read8 | absent | MP/FP write8 |
| integer/big arithmetic/bit/shift | rhs scalar read | lhs scalar read; AXNON значит чтение D | MP/FP write result |
| conditional W/L/F branches | rhs scalar read | explicit lhs scalar read | AIMM absolute Dis-PC |
| JMP | absent | absent | AIMM absolute Dis-PC |
| RET/EXIT/NOP | absent | absent | absent |
| CASE/CASEL/CASEC | int32/big/string-ref read | absent | AIMM typed table_id |
| GOTO | int32 index read | absent | AIMM goto table_id |

Каждый read/write проверяется на width/alignment и object bounds до исполнения.
При AXNON у арифметики destination обязан быть и readable, и writable; у branch
такой implicit lhs не допускается, поскольку D — PC, не scalar. Immediate W/L
знаково расширяется из signed30; float literal не кодируется через AIMM.
Для shifts source count имеет width4 даже у big; result/lhs — width4/8.

Каждый PC строго меньше instruction_count. Table ID меньше числа таблиц,
kind совпадает с opcode. Loader сохраняет typed immutable table отдельно от
module data. JIT cache native PCs принадлежит code generation и не меняет
wire tables; interpreter/JIT оба обращаются к проверенному Dis-PC. Обычный
uint32 table_id сверх signed30 range не становится адресуемым автоматически.

Для первого whitelist беру только уже существующие номера ISA, без новых
opcode IDs. FRAME/CALL/MCALL/SPAWN/SEND/RECV/ALT, pointer moves и indirect
addresses нельзя считать готовыми по этому framing: им нужны собственные
role/signature/GC rules и отдельное согласование. Возвращаемый entry fixture
не должен использовать старые NC64 capture offsets как новый public ABI.

## Imports/exports: предлагаемый первый callable subset

Предлагаю начать с typed scalar signatures, а не с opaque32 hash. Candidate
signature version1: result_kind:u32, parameter_count:u32, затем parameter kinds
u32. Candidate kinds: void0 (только result), int32=1, big=2, real=3, FIR bool=4.
Это предложения номеров signature namespace, не уже принятые constants.
Record version и byte extent должны быть явными; неизвестная version/kind
отвергается. Ref/ADT/array/list/chan/module/function shapes пока не считаются
эквивалентными этому scalar subset. Полный структурный type graph потребует
согласованного расширения подписи; совпадение hash само по себе недостаточно.

Imports: count:u32; record module_name_id, symbol_name_id, signature_bytes,
signature payload. Exports: count:u32; record symbol_name_id, entry_pc,
frame_type_id, signature_bytes, signature payload. Все поля здесь u32 BE,
кроме вложенного signature payload с явным extent. Ordinal record IDs можно
использовать для module-call resolution; фактические CALL/MCALL roles ещё
отдельно согласовываем. Duplicate export names и ambiguous resolution reject.
Loader проверяет ABI/extensions/interface dependency graph до heap publication;
путь файла не является wire authority. Для первого fixture imports/exports
отсутствуют, а не имитируются старым signature hash.

## Data initializers и relocations: конкретный вариант

Предлагаю data section: initializer_count:u32, затем записи offset:u32,
value_kind:u32 и фиксированный payload width по согласованному kind. На первом
шаге только scalar kinds с BE i32/big/IEEE-real/bool bits. Номера kind надо
принять вместе с signature vocabulary, не ввести самостоятельно в emitter.
Count0 означает default initialization, не отсутствие runtime data_bytes.

Runtime allocates data_bytes по descriptor. Обычные bytes сначала zero; каждый
отмеченный ref slot инициализируется runtime nil до GC publication. Это не
утверждение, что native nil равен all-zero bytes, и не назначение wire nil ID.
Scalar initializer aligned, полностью помещается и не пересекает ref slots;
перекрывающиеся initializers отвергаются. String/ref/function/module values
позже получают typed initializer kinds, а не serialized native pointer.

Relocations не переписывают wire numeric data в host pointers. PC остаётся
Dis-PC, string/table/type/import references — проверенные IDs. Runtime строит
отдельные resolved objects. Исчерпывающий перечень relocation sites нужен
перед тем, как разрешать соответствующие instructions. Hash/адрес C-функции
не заменяет структурный import contract.

## Первый настоящий executable golden — предлагаю для согласования

Module name pool index0; executable/profile1; data_bytes4; data_type_id0;
entry_pc0; entry_frame_type_id1. Types: descriptor0 = size4/align4/map00;
descriptor1 = size64/align8/map00. Strings: одна nonempty module-name запись.
Data: initializer_count0. Imports/exports/control tables отсутствуют.

Code instruction_count2: MOVW #42, MP[0]; EXIT. При сохранённом ISA framing
candidate code bytes: `00000002 2d102a00 0f1b`. Это code payload proposal,
не готовый executable файл: first semantic reader, module allocation, entry
setup и execution path ещё должны быть реализованы/проверены. Host acceptance
читает int32 MP[0]==42, а не старую private word8 capture convention.

После принятия этих дополнительных правил: вручную проверенный whole-module
fixture, semantic readers, interpreter, затем JIT с теми же bytes. Перед C/ref
calls дополнительно обязательны GC/result-slot/unwind fixtures.

## Разделение параллельной работы

Со стороны FlashVM беру согласованный string-pool codec с reader/writer/getter,
его независимые fixtures, C99/CLI envelope integration, установку SDK и примеры.
Module/types/code helpers и production execution можно продолжать в Inferno
после ответа на дополнительные предложения выше. Явно сообщаю это через
журнал, чтобы не повторить одновременную реализацию Rune comparator.
