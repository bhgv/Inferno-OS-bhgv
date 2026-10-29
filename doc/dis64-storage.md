# Dis64: scalar initialization и interfaces v1

Дата: 2026-10-30. Основания: FLASHVM-0008/0009 и INFERNO-0011.
Это codecs и structural validation, не включение runtime calls/GC/entry.
Registry ABI1 и scalar kinds0..4 сохраняются.

## Data initializers

Payload: count:u32 BE, затем count записей offset:u32, kind:u32 и payload
фиксированной ширины: int32/bool4, big/binary64 bits8. Void/unknown kind
запрещены. Bool имеет только0/1. Integer/real bits не проходят signed casts,
float conversion, normalization или canonicalization NaN.

Record полностью помещается в object_bytes, offset кратен width. Никакие две
записи не перекрываются; порядок записей произволен, сортировка не требуется.
Нельзя затрагивать даже вторую половину mapped ref64. Descriptor и его полная
map8 проверяются до чтения/записи результата; bytes payload расходуются точно.
Writer проверяет все records, maps и capacity до первой записи. Inputs/output/
written storage не перекрываются. У int/bool input high32=0; на wire этого поля нет.
Count/byte extents и runtime allocation limits — разные ограничения. Scalar
pilot ограничивает initializer count1024 как implementation resource policy;
общий reader принимает явный limit, не новый wire максимум.

`dis64initializersapply` принимает проверенную immutable view и unpublished
native ABI1 storage. Он повторно проверяет всю запись до изменения destination,
zeroes object bytes, затем пишет caller-supplied runtime nil bytes в каждый
mapped slot, после чего размещает скалярные payload bits в native byte order.
Runtime nil не предполагается all-zero или serialized pointer. Source maps,
payload, view, nil bytes и output storage независимы. Capacities и nil presence
проверяются до записи. Caller не публикует storage для GC до успешного возврата.

Apply ограничен native pointer64/uint32/uint64 host representation и не является
cross-target endian emitter. Wire writer работает отдельно и всегда пишет BE.
Это не выделение heap, не retain/release, не автоматическое создание Type/Module
и не доказательство работы active collector с map8. Для реальных FP значений
runtime должен иметь заявленный IEEE binary64 representation.

Generic initializer helper проверяет mapped refs и умеет инициализировать их nil.
Scalar pilot по-прежнему отвергает mapped references в MP/Frame: GC acceptance
не следует из правильных bytes одного unpublished buffer.

## Signature record и external extent

Signature v1 — version:u32=1, result_kind:u32, parameter_count:u32, затем
parameter_count kinds:u32. Внешний signature_bytes включает всю запись:
12+4*parameter_count. Void0 разрешён только для результата; параметры1..4.
Unknown version/kind, truncation, trailing bytes и count/resource overflow reject.
Exact structural equality сравнивает проверенные canonical records, не hash.
ABI/extensions всё равно должны совпасть на уровне checked dependency graph.

## Imports и exports

| Section | Records после count:u32 |
| --- | --- |
| imports | module_name_id:u32, symbol_name_id:u32, signature_bytes:u32, signature |
| exports | symbol_name_id:u32, entry_pc:u32, frame_type_id:u32, signature_bytes:u32, signature |

Все имена — checked non-nil ordinal IDs actual string pool. Содержимое может
совпадать у разных pool IDs. Export PC меньше instruction_count, descriptor
frame существует, имеет size>=64/alignment8. Полный расход sections обязателен.

Два exports с одинаковым содержимым symbol name запрещены. Same module/symbol
imports с exact same signature могут иметь разные ordinal aliases; conflicting
signatures отвергаются как ambiguous. Different module names независимы.
Pair checks используют actual signed-Rune pool equality, не numeric ID и не hash.
Эта точная политика также передана FlashVM в журнале для прямой сверки.

Views/getters immutable, readers/writers атомарны на отказе. Поиск и pair checks
линейные/квадратичные; explicit resource limits обязательны. Runtime вправе
строить отдельный checked index без изменения wire IDs.

## Граница структурной проверки

Binding frame-shape check НЕ проверяет размещение arguments/locals, safe points,
root coverage, callee cleanup или callable dependency graph. Frame64 со scalar
signature ещё не разрешён к реальному вызову только по этим codecs. Production
CALL/MCALL, reference/recursive signatures, module linking/publication и
ref-result unwinding остаются закрыты до отдельных правил и исполнения.
Scalar pilot пока отвергает imports/exports/control sections, даже если отдельно
их structural records корректны. General library metadata этим не запускается.

## Проверки и реальные scalar paths

```
python3 libinterp/tests/dis64-storage.py
python3 libinterp/tests/dis64-payloads.py
python3 libinterp/tests/dis64-pilot-native.py
```

Storage fixture сверяет independent Python bits/geometry/structural-name oracle,
exact writer bytes, native endian int/big/NaN/signed-zero, supplied nil pattern,
capacity/truncation/no-overlap/ref-slot rejection и unchanged outputs.

Новый [pilot-init42.hex](../libinterp/tests/dis64/pilot-init42.hex) —192-byte
whole module: initializer MP[0]=40 и ADDW #2, MP[0]; EXIT. Shared writers дают
побайтно тот же hand-written golden. Actual C/RX fixture применяет real init
codec и production xec ADDW / AMD64 RX WORD32 и получает42, сохраняя guards.
Оригинальный180-byte MOVW42 golden также продолжает проходить.

EXIT только декодирован. Это не production module loader/entry/scheduler run;
active WORD host-long boundary и error/unwind EXIT ещё требуют интеграции.
Physical ARM64 run и new-ABI calls/GC/ref-result acceptance здесь не заявляются.
