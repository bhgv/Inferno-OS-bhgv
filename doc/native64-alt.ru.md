# Native64: заимствованные каналы в блокирующем ALT

Checked LOAD профиль допускает блокирующий ALT с 1–32 простыми guard’ами
для int/string. Каналы берутся из mapped FP/MP slots либо из проверенного
Window.ctl. Таблица использует заимствованные 8-байтные channel/payload
addresses, без новых retain; владеющие аргументы или Window остаются roots.
Четырёхбайтные nsend/nrecv и selection word сохраняют ширину.

Обычный компилятор выдаёт IMOVW для borrowed channel copy и ILEA для payload
address. Private roles189/192 расширяют copy до IMOVL с проверенной геометрией;
role190 публикует полный адрес через доверенный native64localbind до C/JIT
исполнения. Callback проверяет границы и maps источника/назначения и
принадлежность FP текущему активному кадру стека. Role191 связывает ALT с точной последовательностью подготовки,
заголовками, selection slot и следующим IGOTO. Это не изменение wire opcode
или публичного ABI Alt/Altc. Обычный LEA и 32-битные пути сохраняются.

Loader требует unmapped borrowed table, mapped channel/owner, верный payload
map, уникальные slots и полные bounds. Запрещены call islands, ветвления и
case targets внутрь подготовки; selection не может пересекать таблицу или
payload. IGOTO должен читать именно selection и иметь ровно столько targets,
сколько guard’ов. Compiler CFG также закрывает вход внутрь подготовки.
ALT labels проверяются как ordinal-owned targets, без numeric case AST bounds.

Готовые и блокирующие операции исполняются существующими alt.c и scheduler,
через обычный C/JIT bridge. Нового native ALT fast path здесь нет. Обычные
SEND/RECV handlers сохраняют refcount String и семантику буферов/очередей.

## Проверки

`native64-alt.py` строит 21 полное Limbo body; независимый classic oracle
обнаруживает 180 повреждений новых records и сверяет operands, maps,
дескрипторы и таблицы переходов. Проверен предел32/33 guard’ов. NBALT,
массивы каналов, computed channels и payload кроме int/string остаются закрытыми.

1296 C/JIT запусков с quanta1/3/1000 проверяют:

- два receive/send guard’а, FP/MP channels и Window.ctl;
- FIFO/round-robin, готовые/блокирующие peers, buffers0/1/3;
- дублирующиеся guard’ы одного канала и отмену всех регистраций;
- signed int границы и nil/empty/ASCII/Rune/NUL String;
- точный выбранный ordinal, один transfer и неизменность другого output;
- сохранность соседних Word bytes, опустошение queues и buffers;
- полное освобождение Window/channel/message, кадров, modules и environments.

20 malformed metadata случаев отклоняются до allocation. Все 41/42 точки
отказа конструктора C/JIT откатываются. Active controls смещают Window field
и удаляют проверку borrowed table map: оба дают semantic predicate failure
без sanitizer crash. Локально прошли UBSan и ASan/UBSan
(`ASAN_OPTIONS=detect_leaks=0`; ref/allocation retirement проверяет fixture).

Gate включён в Linux amd64/arm64/riscv64, Android ARM64 Bionic CI и общий
native64-port.py. Локальное исполнение относится к amd64; CI других
архитектур здесь не объявляется выполненным.

## Startup frontier

Настоящий awm.init проходит PC140 Window.ctl и останавливается на PC142
Wmcontext.ctl с alt-field-channel. Полный command остаётся accepted.
Остальные поля Wmcontext, составные payload и полный event loop ещё впереди.
Полного графического AWM и Android UI пока нет.

Локальные регрессии прошли: typed local calls (384 C/RX), runtime binding
(1536 отказов), Window fields (378), SEND/RECV (2592), полный emuinit/Arg
(48), compiler body controls, чистая amd64 console/paired boot и 324 настоящих
C/JIT boot-процесса. Все 16 classic startup artifacts сохраняют обычные bytes.
