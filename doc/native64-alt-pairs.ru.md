# Native64: пара (ref, chan of string) в blocking ALT

Этап после Wmcontext ALT: `awm.init` принимает выбранную пару
`(ref Wmsrv->Client, chan of string)` из `join`. Следующий барьер настоящего
исходника — PC153 `alt-payload-class`, тройка из `req`. Полный event loop,
графический AWM и Android UI пока не запущены.

## Контракт

Принимается только точный Ttuple из двух полей: Tref и Tchan(Tstring).
Classic layout 8 bytes, native layout 16 bytes, alignment 8, два GC roots.
Другие пары и тройки остаются закрыты. Classic Dis bytes и 32-битный код
не меняются.

Private role 195 переносит адрес пары в borrowed ALT table; role 194
обозначает IMOVMP для распаковки выбранного tuple. Проверяются оба source
и destination roots, полные spans, принадлежность ALT island, отсутствие
пересечения selection slot с payload и call sites с mapped copy.
Контракт pair-address 48 отделён от обычного pointer-address 24, чтобы
сужение размера не прошло проверку метаданных.

Mapped copy использует существующий native Type 16/map C0. Если модуль
также создаёт канал такой пары, allocation и copy используют один descriptor.
Его индекс учитывает уже выделенные imports/byte arrays/triple descriptors.
ALT и канальные mover/cleanup исполняются существующим runtime; новая
поддержка не вводит отдельной реализации каналов.

## Проверка

`utils/tests/native64-alt-pairs.py --limbo PATH` строит 12 полных Limbo bodies
и независимо сверяет classic operands, descriptors, tables, maps и .m hash.
84 повреждения records обнаруживаются oracle. Отдельно принимается исходник
с настоящим Wmsrv.Client; соседние неподдерживаемые tuple-типы отвергаются.

576 запусков C/JIT проверяют FP/MP tuples, mapped projection, shared descriptor,
четыре сочетания nil roots, порядок sender/receiver, duplicate guards,
FIFO/round-robin, quanta 1/3/1000, buffers 0/1/3, точные reference counts,
содержимое объектов, вытесненные outputs, отмену registrations, очистку
buffer roots и полное освобождение frames/modules/environments.

140 повреждений метаданных отвергаются до allocation. Все 29/30 точки
отказа конструктора C/JIT откатываются. Два активных контроля намеренно
обрезают mapped copy до одного поля и снимают проверку второго GC root;
оба обязаны упасть на смысловом predicate без sanitizer error.

Gate включён в native64-port.py и workflow Linux amd64/arm64/riscv64,
а также Android ARM64 Bionic. Локальные исполнения относятся к amd64;
наличие gate в CI не означает выполненный здесь запуск других архитектур.

Локально прошли UBSan и ASan/UBSan, включая оба active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает owners/allocations).
Регрессии: Wmcontext ALT 1734, прежний ALT 1296, mapped channel allocation
1938 C/JIT сценариев с их controls. Чистая amd64 console/paired boot сборка
и 324 настоящих emuinit C/JIT процесса прошли; AWM window не запускался.
