# Native64: checked primitive channel SEND/RECV

Checked LOAD lowering принимает ISEND/IRECV для byte, int/fix, big, real
и managed root payload (String, Ref, Chan, Array, List, Module). Channel и
value должны находиться в прямых FP/MP cells; маленький scalar SEND также
может использовать immediate. Aggregate payload, ALT и receive сразу в
borrowed caller result address остаются закрытыми.

Проверяются исходный channel payload type, operand classes и native spans.
Private checked role188 хранит width/root contract1/4/8/24; это не новая
serialized instruction. Materializer сохраняет обычный opcode/адреса и
убирает proof-only middle value. Owning validator проверяет source/destination
root maps, границы/alignment и запрет transfer внутри call setup island.
Classic bytecode и 32-bit code paths сохраняются.

Выполнение использует production isend/irecv, queue helpers, buffered
read/refill и movers. Blocked sender/receiver сохраняет pointer в живом
frame или Module MP, peer передаёт значение и будит Prog. Mapped transfer
удерживает новое значение, освобождает displaced destination и очищает
consumed buffer roots. Buffer refill сохраняет нужных временных владельцев.

AMD64/ARM64 pointer-free buffered JIT использует production cchanleaf и
native copy/publication. Mapped payload, blocking и queued peer идут через
retaining VM path. RV64 сохраняет свой существующий C bridge; assert на
выбор leaf относится к AMD64/ARM64, а value/ownership checks общие.

## Проверки

`native64-channel-transfer.py` строит 22 полных Limbo bodies. Независимый
classic oracle обнаруживает 72 повреждения transfer records и сверяет
operand relocation, root classes и width contracts. Boundary отдельно
допускает все шесть managed root classes и отвергает aggregate/borrowed
receive forms. Source bytes совпадают с обычной компиляцией.

2592 C/JIT runs с quanta1/3/1000 проверяют:

- String/int/byte/big/real payload, обе signed Word границы и полные 64-bit bits;
- nil/empty/Rune/embedded NUL String, `-0.0` и quiet NaN bits;
- FP/MP channel и value operands, FP receive temporary и MP destination;
- FIFO/round-robin, sender/receiver first, rendezvous и buffers0/1/3;
- несколько queued senders, full-buffer refill, wakeup и parent exit;
- nil-channel errors, точное число завершённых processes и пустые queues;
- native pointer-free leaf и mapped fallback selection на AMD64/ARM64;
- exact payload/count bytes, untouched соседние MP bytes и upper-lane guards;
- displaced String, buffer-root cleanup и полное освобождение payload,
  channel/array Types, stacks, environments, modules и shared group refs.

54 malformed metadata случая отказывают до allocation с неизменными owner/
REG. Все 42/43 constructor allocation failures проверены на откат. Active
controls искажают materialized payload offset и удаляют mapped-value guard;
каждый обязан дать semantic predicate failure без sanitizer crash.

Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI и в
native64-port.py. Локальное исполнение относится к amd64. ALT, составные
передаваемые payload и полный GUI этим gate не проверяются.

## Startup frontier

Неизменённый awm.command проходит ISEND `sync <-= nil` на PC1401. Первый
отказ теперь PC1404 ICONSW — построение `list of {0,1,2}` для Sys.pctl.
AWM init всё ещё требует полного admission command. Wmsrv wm остаётся
на typed-span PC40 перед event loop. Полный AWM, Sh service и Android UI
ещё не запущены.

Spawn boundary обновлён: primitive receive child теперь поддерживается;
негативный случай проверяет child с aggregate receive.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; refs/allocations проверяются fixture).
Регрессии: checked spawn, borrowed list prefixes и полный emuinit/Arg.
Чистая amd64 console, paired boot и 324 настоящих C/JIT boot-процесса прошли.
Все 16 classic startup artifacts сохраняют обычные bytes.
