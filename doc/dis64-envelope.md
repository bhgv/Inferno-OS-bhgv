# Dis64: envelope ABI1

Registry принят обеими сторонами: [dis64-registry.json](dis64-registry.json),
FLASHVM-0004 и INFERNO-0008. Общие правила: [contract](dis64-contract.md).
`include/dis64.h` — работающий кодек envelope; **это не module loader**.

## Fixed header и directory

Все числа BE, без padding; все offsets измеряются от первого байта файла.

| Offset | Поле | Bytes / значение |
| ---: | --- | --- |
| 0 | magic | 8: 44 69 73 36 34 0d 0a 1a |
| 8 | format_major | u16: 1 |
| 10 | format_minor | u16: 0 |
| 12 | header_bytes | u32: fixed header + manifest + directory |
| 16 | abi_id | u32: 1 |
| 20 | flags | u32: 0 |
| 24 | required_extension_count | u32 |
| 28 | section_count | u32 |

Сразу после fixed header идут extension records: name_bytes:u16, major:u16,
minimum_minor:u16, reserved:u16=0, name bytes. Имя содержит 1..255 ASCII bytes,
начинается с a-z, остальные bytes из a-z/0-9/точка/подчёркивание/дефис.
Codec проверяет этот синтаксис и строгий порядок имён; registry/support callback
проверяет смысл namespace и совместимость версии. Имена не заканчиваются NUL.
Callback read-only: он не должен строить объекты программы или менять вход.

Directory имеет section_count записей kind:u16, flags:u16, offset:u32, bytes:u32.
Header заканчивается ровно после directory. Cursor=header_bytes; каждый offset
равен cursor, затем cursor увеличивается на bytes после проверки границ.
В конце cursor=file_size; gaps/trailing bytes запрещены. Zero-length секция
не двигает cursor. Неизвестные flag bits запрещены для всех kinds.

## Что проверяет кодек

- Точные version/ABI/flags, полный header/manifest/directory, явные resource limits.
- Поддержку каждой mandatory extension через переданный callback.
- Dense extents, known singleton duplicates и отсутствие unknown required kinds.
- Наличие module/code/types с required=1 и непустым payload.
- Known semantic kinds1..8 имеют required=1; debug/provenance имеют required=0.
- Imports/exports/strings/control имеют минимум4 bytes для внутреннего header.
  Codec не декодирует его и не делает выводов по нерасшифрованному count.
- Data payload может быть нулевым. Его связь с module data_bytes проверит будущий
  semantic reader. Debug/provenance остаются opaque: envelope pass не означает,
  что их внутренняя версия или пустой payload семантически допустимы.
- Unknown optional секция может быть пустой и повторяться: codec не придумывает
  для неизвестного kind его singleton/count правила.

File size в API — unsigned32; caller обязан проверить более широкий размер до
сужения. Сам API не выделяет память. Limits extension_count и section_count
задаются вызывающим кодом явно. Результат/view и written length не меняются
при отказе. Borrowed views действительны только пока вход неизменяем и жив.

Writer получает уже собранные opaque payloads и не пересобирает их смысл.
Он проверяет все lengths/flags/имена до записи, вычисляет canonical offsets и
не пишет частичный header при недостаточной ёмкости. Входы writer, его output
и published-length storage должны быть независимы и не перекрываться.
Writer может записать требование feature, которую runtime reader не поддержит;
семантическое согласие о feature не выводится из успешной сериализации.

## Инструмент и fixtures

```
make -C utils/dis64inspect
utils/dis64inspect/dis64inspect container.dis64
utils/dis64inspect/dis64inspect container.dis64 --rewrite copy.dis64
python3 libinterp/tests/dis-envelope.py
```

Inspector ограничивает файл 64 MiB, extensions1024 и sections4096. Он не
реализует VM extensions, поэтому любой manifest с обязательной возможностью
отвергается. JSON всегда явно указывает envelope_only=true и
execution_supported=false. Rewrite сохраняет opaque bytes, не превращает их
в исполняемый Dis64. Существующие classic/NC64 файлы этим инструментом не
конвертируются и действующим Inferno loader не переосмысливаются.

Hand-written minimum golden имеет payloads M/C/T: это метки fixtures, а не
код, module descriptor или executable type. Python oracle и ctypes adapter
проверяют реальные C функции, byte-for-byte output, ext feature policy,
capacity/rejection atomicity, сдвинутые input spans и inspector roundtrip.

Следующий этап — [payload proposal](dis64-payload-proposal.md), совместный ответ
и semantic readers; только затем загружаемый module fixture и execution.
