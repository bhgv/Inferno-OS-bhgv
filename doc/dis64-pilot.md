# Dis64 module/types/code: первый scalar pilot

Дата: 2026-10-29. Framing принят FLASHVM-0006 и
[ответом по payloads](dis64-flashvm-payload-response.md); дополнительные
alignment/role правила подтверждены INFERNO-0009. Registry остаётся ABI1.
Это semantic-reader этап, **ещё не включённый loader/VM target**.

## Metadata и structural types

Module — BE u32: name_string_id, profile0(library)/1(executable), data_bytes;
при nonzero data_bytes далее data_type_id, только у executable далее entry_pc
и entry_frame_type_id. При отсутствии поля нет wire sentinel. Readers требуют
точной длины и сохраняют выход при отказе; writers preflight до первой записи.

Types — count:u32; ordinal entries object_bytes:u32, alignment:u32 и полная
map8 ceil(object_bytes/64). Alignment1/2/4/8, размер кратен alignment, empty
size0/align1. Mapped reference требует alignment8 и полного размещения.
Structural descriptor не является callable signature или структурным type graph.
Типы проверяются с явными count/object limits; текущий helper также наследует
DisMaxOffset=0x7fffffff из layout prototype. Это предел реализации, не обещание
принимать любой u32 размер или новый универсальный предел ABI. Линейный getter не меняет IDs.
Entry frame: bytes>=64, alignment8; этот pilot разрешает только нулевые карты
entry-frame и module data, до настоящих safepoint/GC/ref-result доказательств.

## Code reader: реализованный subset

В начале instruction_count:u32, затем existing opcode/mode byte и compact
signed30 operands: middle, source, destination. Decoder не делает signed shifts
и не сужает unsigned число через implementation-defined cast. Конец потока
обязан совпадать с концом payload; пустой code в этом subset отвергается.
Непрямые modes и неизвестные комбинации не попадают в legacy decoder.

Реализованы MOVW/MOVL/MOVF, integer/big add/sub/mul/div/mod/and/or/xor/shifts,
W/L/F conditional branches, JMP, EXIT. Это граница reader, а не заявление
исполнения этих операций новой VM. INOP0 не считается no-op; RET/calls/ref/
channels/control-table opcodes пока явно unsupported этим reader.

- MP/FP operands — byte offsets; scalar width/alignment и полный span проверяются.
- FP offsets<64 защищены в начальном scalar subset, как runtime Frame area.
- MOVF и float branch operands читают память8; AIMM float не существует.
- Big shift source count имеет width4, lhs/result width8.
- AXNON у арифметики читает D; у branch запрещён, поскольку D — Dis-PC.
- Branch/JMP target — AIMM, nonnegative и строго меньше instruction_count.
- Signed30 immediates для word/big сохраняют диапазон -2^29..2^29-1.

ISA включается один раз вызывающим translation unit перед dis64-code.h.
Typed CASE/GOTO table-ID roles согласованы как направление, но полный control
section framing ещё не реализован этим scalar reader. Неизвестный opcode или
неподдерживаемый role не становится разрешённым через C fallback старого ABI.

## Цельный golden и точная граница evidence

[Текстовые байты](../libinterp/tests/dis64/pilot42.hex): 180 байт, имя Pilot,
profile1, data_bytes4, datatype0, entry0, frametype1. Types: size4/align4/map00
и size64/align8/map00. Data initializer_count0. Code:

```
00000002 2d102a00 0f1b
```

MOVW #42, MP[0]; EXIT. Strings — настоящий pool codec FlashVM с одним lossless
Rune record. Это не прежние marker payloads M/C/T. C fixture производит те же
байты через actual metadata/types/string-pool/envelope writers; вручную заданный
hex проверяется независимо. Межсекционные IDs, type sizes, name, entry и code
roles проверяет dis64pilotcheck до публикации borrowed view.

Этот pilot проверяет executable profile1, no extensions, scalar data/frame,
scalar initializers из [storage framing](dis64-storage.md). Imports/exports/control
sections пока отвергаются как unsupported; nonempty scalar initializers проверяются. General metadata codec принимает profile0,
но code-less library и dependency resolution не объявлены реализованными.

```
python3 libinterp/tests/dis64-payloads.py
python3 libinterp/tests/dis64-pilot-native.py
```

Первый тест проверяет writers/readers/getters, signed operands, role matrix,
capacity/rejection atomicity и whole-module linking. Второй читает весь golden,
проверяет semantic references, применяет scalar initialization и исполняет один
MOVW либо ADDW через production xec.c и actual AMD64 RX в WORD32/ref64 модели.
Для нового192-byte pilot-init42 MP[0] сначала40, затем ADDW2 даёт42.
Он проверяет native bits, MP[0]==42 и guards.
EXIT только декодируется как terminal opcode; production scheduler/EXIT не
вызывается. Harness не объявляется полной VM, bytecode loader или полноценным
compiler-to-runtime acceptance. ARM64 host run этим не заменяется.

Следующий gate: actual module allocation, entry setup, production EXIT/scheduler,
затем скалярные программы/branches, C calls и отдельные GC/result-slot/unwind
fixtures. Ref headers/temps не становятся roots из-за размеров полей.

## Unpublished native Module construction

The newer [runtime adapter](dis64-runtime-build.md) constructs actual Module/Inst
and heap storage from the same semantic pilot. Its connected C/AMD64 RX test now
executes the real decoder/xec loop, IEXIT/error and stack cleanup. The single-leaf
fixture described above retains its narrower scope. Automatic readmod routing,
complete emulator startup/process exit, calling ABI and live GC remain unproven.
