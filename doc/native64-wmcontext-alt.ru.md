# Native64: Wmcontext channels и reference payload в ALT

Checked LOAD профиль читает пять каналов Draw.Wmcontext в заимствованные
слоты блокирующего ALT: kbd, ptr, ctl, wctl и images. Проверяется весь ADT:
семь именованных fields, исходные 28/native 56 bytes, alignment 8, root kinds,
классы payload каналов и две последние ссылки Sys.FD/Draw.Context. Другая
ADT и тот же namespace/layout с неправильным payload отвергаются.

Role 193 задаёт проверенный Wmcontext channel field: offsets 0/4/8/12/16
становятся 0/8/16/24/32. FP/MP owner остаётся mapped root; сама копия в ALT
не добавляет retain. Loader связывает selector с width contract: kbd требует
Word 4, остальные четыре канала — mapped reference 8. Поля connfd/ctxt нельзя
интерпретировать как каналы. Window.ctl сохраняет отдельный role 192.

ALT payload дополнен классом ref. Используется уже проверенный contract 24
и полный адрес mapped slot; ссылочный объект не копируется как aggregate.
Обычный retaining VM mover сохраняет его GC/ownership, включая вложенные
roots. Native64 materializer и существующий C/JIT bridge остаются общими.
Classic opcode bytes, Word 32 и существующие 32-битные пути сохраняются.

## Проверки

`native64-wmcontext-alt.py` строит 22 полных Limbo bodies с независимыми
classic operands/maps/tables/descriptors и generated .m signatures. Oracle
обнаруживает 168 повреждений channel/payload records. Используются реальные
генерируемые C layouts Wmcontext 56, Pointer 16, Image 72 и Context 24.

1734 C/JIT сценария с quanta 1/3/1000 проверяют:

- 1440 запусков пяти разнородных guard’ов через FP/MP owners;
- 288 запусков прямых ref channels с повторяющимися guard’ами;
- шесть nil-owner ошибок до регистрации каналов и полный unwind;
- FIFO/round-robin, готовые/блокирующие peers и buffers 0/1/3;
- signed Word границы, nil/empty/ASCII/Rune/NUL String;
- точное тождество и содержимое Pointer/Image, nested String внутри Image;
- истинные owners каналов, сохранность и освобождение connfd/ctxt roots;
- вытесненные outputs, отсутствие лишних writes, пустые queues/buffers;
- полное освобождение heap nodes, Type, frames, modules и environments.

110 повреждений metadata отвергаются до allocation; все 46/47 точки отказа
конструктора C/JIT откатываются. Active controls подменяют channel field
и удаляют selector/payload class join; каждый обязан дать semantic predicate
failure без sanitizer crash. Первый отдельно обнаруживает оставшиеся
заблокированными процессы.

Локально прошли UBSan и ASan/UBSan с обоими controls
(`ASAN_OPTIONS=detect_leaks=0`; refs/allocations проверяет fixture).
Регрессии: прежний ALT 1296, SEND/RECV 2592 и полный emuinit/Arg 48.
Чистая amd64 console/paired boot и 324 настоящих C/JIT boot-процесса прошли;
все 16 classic startup artifacts сохраняют обычные bytes.

Gate включён в Linux amd64/arm64/riscv64, Android ARM64 Bionic workflow
и native64-port.py. Локальное исполнение относится к amd64; CI для других
архитектур здесь не объявляется выполненным.

## Startup frontier

Настоящий awm.init проходит Wmcontext.ctl/kbd/ptr и reference receive slot
на PC149. Следующий отказ — PC151 alt-payload-class: составное значение
(ref Client, chan of string) из join. Весь command остаётся accepted.
Inline aggregates, NBALT, computed/array channels и полный event loop
по-прежнему требуют дальнейшей работы. Графический AWM/Android UI ещё не запущены.
