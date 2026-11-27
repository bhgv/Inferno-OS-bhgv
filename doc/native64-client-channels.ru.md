# Native64: поля и каналы Wmsrv.Client

Настоящий awm.init проходит отправку `z.ctl <-= "exit"` на PC168 и чтение
`z.znext` на PC169. Следующий отказ — PC178 `context-nil-roles` при сравнении
`win.image` и `screen.image`. Command остаётся accepted. Полный AWM event loop,
графическое окно и Android UI пока не запущены.

## Проверяемый layout

Принимается полный nominal Wmsrv.Client: 13 полей, classic size 52,
native size 96, alignment 8, GC map F7/90. Методы не занимают storage.
Проверяются имена, порядок, scalar/root classes, вложенные payload types,
ref Draw.Pointer/Wmcontext/Point/Image и ref Wmsrv.Window/Client.
Изменение даже неиспользуемого `images` отвергает передачу через `stop`.

| Поле | Classic offset | Native offset | Тип |
| --- | --- | --- | --- |
| kbd | 0 | 0 | chan of int |
| ptr | 4 | 8 | chan of ref Pointer |
| ctl | 8 | 16 | chan of string |
| stop | 12 | 24 | chan of int |
| flags | 16 | 32 | int32 |
| cursor | 20 | 40 | string |
| wins | 24 | 48 | list of ref Window |
| znext | 28 | 56 | ref Client |
| images | 32 | 64 | chan of (ref Point, ref Image, chan of int) |
| id | 36 | 72 | int32 |
| fid | 40 | 76 | int32 |
| token | 44 | 80 | int32 |
| wmctxt | 48 | 88 | ref Wmcontext |

Classic bytes и 32-битный код не меняются. Каналы остаются borrowed fields:
очередь и payload owners обслуживает существующий runtime. Aggregate SEND/RECV
через images не открывается этим этапом.

## Материализация

Private role 204 обозначает SEND/RECV через один из четырёх scalar/root
каналов Client. В checked record сохраняются displacement и payload contract;
в native Inst они разделяются на indirect operand и обычный runtime opcode.
Roles 205/206 читают и записывают root/int32 fields через прямой FP/MP owner.
Разрешены только точные field offsets; padding не становится полем.

До allocation проверяются opcode, режимы, alignment, extent, GC root владельца,
GC class значения, field/payload join, границы immediate и отсутствие пересечения
с call sites. Чтение/запись двух косвенных operands одним field MOV не открыто.
JIT исполняет те же существующие channel/field operations.

## Проверки

`utils/tests/native64-client-channels.py --limbo PATH` строит 36 полных Limbo
bodies с canonical .m signatures и независимым classic operand/map oracle.
270 повреждений records обнаруживаются oracle; похожие ADT, изменённые scalar,
channel и nested images payloads отвергаются.

4032 C/JIT исполнения проверяют четыре канала через FP/MP owners, оба порядка
sender/receiver, immediate int sends, quanta 1/3/1000, FIFO/round-robin,
buffers 0/1/3, nil/empty/ASCII/Rune/NUL String и Pointer payloads. Во всех
сценариях читаются и записываются 13 полей Client. Проверяется весь 96-byte
объект, включая padding, signed Word min/max/negative/zero, upper Word guard,
сохранность channel identities, reference counts, снятие регистрации peers,
очистка root buffers и полное освобождение frames/objects/Types/modules.
Отдельно проходят восемь C/JIT nil-owner/nil-channel ошибок с полным unwind.

450 повреждений метаданных отвергаются до allocation; все 62/63 точки отказа
конструктора C/JIT откатываются. Три active controls намеренно расширяют запись
flags до 8 bytes, подменяют kbd-канал на stop и снимают nonroot GC proof.
Каждый обязан упасть на semantic predicate без sanitizer crash.

Gate включён в native64-port.py, Linux amd64/arm64/riscv64 и Android ARM64 Bionic
workflow. Локальное исполнение относится к amd64; другие архитектуры здесь
не объявляются проверенными.

Локально прошли UBSan и ASan/UBSan со всеми тремя active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает owners/allocations).
Регрессии: обычный channel transfer 2592, FileIO mixed ALT 2304 и request ALT
1152 C/JIT сценариев со всеми controls. Matcher mapped-value control обновлён
для packed field/payload contract. Чистая amd64 console/paired boot сборка и
324 настоящих emuinit C/JIT процесса прошли; графическое окно не запускалось.
