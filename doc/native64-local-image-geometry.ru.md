# Native64: локальные Rect/Point и поля Image

Настоящий reshaped проходит PC716 (чтение Image.r в локальный Rect) и
доходит до PC762 `load-nil-roles`: проверки списка `z.wins` на nil.
Поэтому awm.init пока остаётся на PC180 `multi-callee` при вызове reshaped.
Command остаётся accepted. Полный AWM event loop, окно и Android UI ещё
не запущены.

## Геометрия и перенос

Rect и Point состоят из четырёх и двух 32-битных чисел. Их classic/native
размеры одинаковы: 16/8 bytes, alignment 4, без GC roots. Новая поддержка
копирует обычные FP/MP значения и выбранные поля полного Draw.Image layout.

| Поле | Offset | Размер |
| --- | --- | --- |
| r | 0 | 16 |
| clipr | 16 | 16 |
| r.min | 0 | 8 |
| r.max | 8 | 8 |
| clipr.min | 16 | 8 |
| clipr.max | 24 | 8 |

Private roles 208/209 читают Rect/Point из Image, 210/211 записывают их,
212 копирует обычную геометрию FP/MP. В checked middle находятся field offset
и byte count; в native Inst они разделяются между indirect operand и reg.

Компилятор доказывает nominal Draw.Image и его полный layout, точные типы
Draw.Rect/Point, все Word leaves и непрерывность обоих spans. До allocation
проверяются opcode, modes, alignment, byte count/field join, extent, GC root
владельца и отсутствие GC roots на всём числовом диапазоне, включая его хвост.
Immediate operands, высокие посторонние bits и пересечение с call sites
отвергаются. Чтение/запись двух косвенных operands одной копией не открывается.

Imported geometry arguments остаются на прежнем отдельном пути. Scalar Image
fields, например depth, этим этапом не открыты. Classic Dis bytes и 32-битный
код сохраняются. Исполнение использует существующий IMOVM в C и JIT.

## Проверки

`utils/tests/native64-local-image-geometry.py --limbo PATH` строит 27 полных
Limbo bodies с canonical .m signatures и независимым classic operand/map
oracle. Проверяются все пять copy roles; 408 повреждений records обнаруживаются.

972 C/JIT исполнения: чтение/запись шести geometry fields, FP/MP owners и
значения, 4-byte-aligned MP spans, обмен Rect, перенос Point между половинами
и alias. Проверяются signed Word boundary bit patterns, весь 72-byte Image,
все соседние MP bytes, caller guard, сохранность display/screen/iname и их
reference counts. Quanta 1/3/1000 включают stack growth и частые возвраты
в scheduler. В число запусков входят 162 nil-owner unwind; проверяются также
видимые копии в mirror до последующей ошибки записи через nil Image.
Frames/modules/objects/Types/environments полностью освобождаются.

952 повреждения метаданных отвергаются до allocation; все 63/64 точки отказа
конструктора C/JIT откатываются. Три active controls подменяют r на clipr,
расширяют Point store с 8 до 16 bytes и пропускают raw GC proof хвоста Rect.
Каждый обязан дать semantic predicate failure без sanitizer crash.

Gate включён в native64-port.py, Linux amd64/arm64/riscv64 и Android ARM64
Bionic workflow. Локальные исполнения относятся к amd64; другие архитектуры
здесь не объявляются проверенными. Реальная графическая Draw-служба не нужна
для тестов числовых копий и владения объектами.

Локально прошли UBSan и ASan/UBSan со всеми тремя active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает owners/allocations).
Регрессии: Draw fields/calls 1162 и field reference identity 2736 C/JIT
сценариев со всеми controls. Прежний negative probe для Image.r заменён
на scalar depth, поскольку Image.r теперь положительно проверяется новым gate.
Чистая amd64 console/paired boot сборка и 324 настоящих emuinit C/JIT процесса
прошли; графическое окно не запускалось.
