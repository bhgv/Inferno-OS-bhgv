# Native64: Sys.FileIO и смешанные сообщения ALT

Этап после request triple: настоящий `awm.init` проходит оба поля FileIO,
четырёхпольные payloads, всю подготовку внешнего blocking ALT и IGOTO.
Следующий отказ — PC168 `indirect-or-reserved-operand`: отправка строки
`"exit"` через поле `Client.ctl` (`z.ctl <-= "exit"`) в обработке события.
Полный event loop, графический AWM и Android UI пока не запущены.

## Геометрия

Проверяется полный nominal Sys.FileIO: ровно два channel fields `read` и
`write`, classic offsets 0/4, native offsets 0/8, Type size 16/map C0.
Проверка включает типы сообщений обоих каналов, даже если используется один.

| Сообщение | Classic offsets | Native offsets | Native size/map |
| --- | --- | --- | --- |
| read: `(int, int, int, Sys->Rread)` | 0, 4, 8, 12 | 0, 4, 8, 16 | 24 / 20 |
| write: `(int, array of byte, int, Sys->Rwrite)` | 0, 4, 8, 12 | 0, 8, 16, 24 | 32 / 50 |

Maps указаны в hex. Rread — chan of (array of byte, string), Rwrite — chan of
(int, string). Числа остаются 32-битными. Padding — raw bytes, не GC roots.
Похожие ADT, неправильные имена, array of int и другие reply channel types
остаются закрыты. Classic Dis bytes и код 32-битной сборки не меняются.

## Перенос и исполнение

Private role 198 читает borrowed channel field через прямого FP/MP owner.
Write-address 199 и read-address 203 имеют отдельные contracts 104/120.
Число 200 уже занято существующим result binding 128|72 и не переиспользуется.
Подтверждаются точный channel/payload join и принадлежность полной ALT island.

Roles 201/202 распаковывают выбранное сообщение через IMOVMP с отдельными
Type 32/50 и 24/20. Проверяются classic descriptor, точные leaf types/offsets,
оба source/destination spans, alignment, каждый root и каждый nonroot/padding
GC slot. Метаданные проверяются до allocation, включая call-site ownership,
selection overlap, сырой ALT table и таблицу IGOTO. Runtime address binder
повторно проверяет весь payload и native frame/module data GC map.

Новые descriptor pools находятся после существующих bytes/channel-result/
mapped-pair/mapped-triple и перед Context, сохраняя его положение последним.
ALT, mapped mover, channel queues и cleanup используют существующий runtime;
JIT входит в тот же checked bridge.

## Проверки

`utils/tests/native64-fileio-alt.py --limbo PATH` строит 21 полный Limbo body.
Независимый oracle сверяет classic operands/maps/descriptors/tables и generated
.m hashes; обнаруживаются 264 повреждения records. Чужой ADT и поддельный
Sys.FileIO с неправильным вторым полем отвергаются при выборе первого.

2304 C/JIT исполнения проверяют оба канала, FP/MP owners и tuples,
receiver-first/sender-first, duplicate guards, signed Word min/max/negative/zero,
восемь сочетаний nil array/replies, empty/nonempty byte arrays и их содержимое,
untouched outputs, вытесненные roots, buffers 0/1/3, quanta 1/3/1000,
FIFO/round-robin, отмену registrations и очистку всех buffer roots.
Проверяются точные refs объектов/Type и полное освобождение frames/modules/
environments. Ещё четыре C/JIT сценария проверяют nil-owner error до регистрации
каналов и полный unwind.

576 повреждений метаданных отвергаются до allocation; все 43/44 точки отказа
конструктора C/JIT откатываются. Active controls обрезают mixed projection,
подменяют FileIO channel field и пропускают nonroot GC proof. Каждый обязан
дать semantic predicate failure без sanitizer crash.

Gate включён в native64-port.py, Linux amd64/arm64/riscv64 и Android ARM64
Bionic workflow. Локальное исполнение относится к amd64; запуск других
архитектур здесь не объявляется выполненным.

Локально прошли UBSan и ASan/UBSan с тремя active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает owners/allocations).
Регрессии: request ALT 1152, Wmcontext ALT 1734 и three-channel results 400
C/JIT сценариев с их controls. Чистая amd64 console/paired boot сборка и
324 настоящих emuinit C/JIT процесса прошли; графическое окно не запускалось.
