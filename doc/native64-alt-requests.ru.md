# Native64: request tuple в blocking ALT

Этап после join-пары: принимается ровно `(ref, array of byte, chan of (int,
string))`, включая настоящий `(ref Wmsrv->Client, array of byte, Sys->Rwrite)`
из `req` в `awm.init`. Classic layout остаётся 12 bytes, native layout —
24 bytes, alignment 8, три GC roots. Соседние формы, например array of int,
обратный порядок reply tuple или chan of string, остаются закрыты.

## Перенос и владение

Private role 197 публикует полный 24-byte payload address в borrowed ALT table.
Role 196 распаковывает tuple через IMOVMP. При переносе проверяются точные
classic offsets 0/4/8 и native offsets 0/8/16, размеры и типы всех полей,
canonical descriptor 12/E0, оба direct FP/MP spans и все три source/destination
GC bits. При загрузке проверяются ALT island, selection slot, call-site
пересечения, alignment, span и отдельный контракт address 72. Подмена
этого контракта на pair-address 48 отвергается до allocation.

Распаковка использует существующий Type 24/E0 для mapped-троек. Создание
канала и копирование разделяют descriptor. Его индекс учитывает imports,
byte-array Type, отдельный channel-result triple Type и mapped-pair Type;
Context Type остаётся самостоятельным. Исполнение ALT, mapped mover и
очистка каналов используют существующий runtime в C и через JIT bridge.

Classic Dis bytes и код 32-битной сборки не меняются.

## Проверки

`utils/tests/native64-alt-requests.py --limbo PATH` строит 17 полных bodies.
Независимый oracle проверяет исходные operands, descriptor, tables, maps,
канонический .m hash и неизменность classic bytes; обнаруживаются 84
повреждения tuple records. Отдельный probe использует настоящий Wmsrv.Client.

1152 исполнения C/JIT: FP/MP payloads, mapped projection, восемь комбинаций
nil roots, empty/nonempty byte arrays с проверкой содержимого, pointer и reply
identity, точные owners, старые outputs, FIFO/round-robin, quanta 1/3/1000,
buffers 0/1/3, sender-first/receiver-first, duplicate guards, отмена registrations,
очистка всех трёх roots в буферах и полное освобождение frames/modules/Types.

В том же модуле одновременно существуют пять descriptor pools: byte array,
channel-result triple, mapped pair, mapped request triple и Context. Проверяются
их размеры, раздельные identities и relocation allocation/copy instructions.
168 повреждений метаданных отвергаются до allocation, включая снятие каждого
из трёх source/destination root bits. Все 43/44 точки отказа конструктора
C/JIT откатываются. Active controls обрезают mapped copy до двух полей и
снимают проверку третьего GC root; оба должны дать semantic predicate failure.

Gate включён в native64-port.py, Linux amd64/arm64/riscv64 и Android ARM64
Bionic workflow. Локальное исполнение относится к amd64; другие архитектуры
не объявляются здесь проверенными.

## Startup frontier

Настоящий awm.init проходит PC153 с req tuple. Следующий отказ — PC154
`alt-field-channel`: `wmrectIO.write` через Sys.FileIO. Далее потребуются
также смешанные четырёхпольные FileIO payloads. Command остаётся accepted.
Полный AWM event loop и графический Android UI ещё не запущены.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`, owners/allocations отдельно считает fixture).
Регрессии: ALT pair 576, Wmcontext ALT 1734, mapped channel allocation 1938
C/JIT сценариев со всеми их controls. Чистая amd64 console/paired boot сборка
и 324 настоящих emuinit C/JIT процесса прошли; графическое окно не запускалось.
