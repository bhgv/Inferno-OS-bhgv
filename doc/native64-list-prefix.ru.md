# Native64: String prefix над существующим List tail

Checked importing-call lowering принимает `"prefix" :: args` непосредственно
при подготовке аргументов, где args — существующий `list of string` в FP/MP.
Это открывает настоящий вызов `Sh.run(ctxt, "{$*&}" :: args)` в awm.command.

Проверяются declared/per-use list element class, исходный mapped root,
compiler temporary `.t`, bounded String cons chain и точный pending frame.
После передачи в mapped formal slot temporary должен немедленно очищаться
canonical readonly nil. Private role187 отделяет borrowed-tail preparation
от прежней nil-initialized роли112. Обычные List operations и classic/32-bit
paths сохраняются.

Owning module проверяет единственный external call island, root maps всех
операндов, общий temporary slot, mapped formal и nil clear. Допускается от
одного до 256 cons; orphan roles и неполная последовательность закрыты до
allocation. Initial MOVP удерживает borrowed tail. Каждый production cons
создаёт новый узел, удерживает прежний tail и освобождает displaced temporary
owner. Caller-owned tail сохраняется; pending frame держит также ранее
подготовленный Context argument.

## Проверки

`native64-list-prefix.py` строит восемь полных Limbo bodies. Независимый
classic oracle обнаруживает 114 повреждений records, сверяет operands/maps
и подтверждает обычные classic bytes. Отдельный source probe проверяет
настоящий Sh.run и его canonical generated signature.

1800 C/JIT случаев с quanta1/3/1000 включают:

- FP/MP tail с длиной0/1/3, один и два новых prefix nodes;
- literal и FP heads, nil, empty String, Rune и embedded NUL;
- existing Context argument, nil Context и точные его owners при allocation;
- fresh, inherited и void native results;
- ошибки первого/второго cons allocation, native callback и missing module;
- сохранность старого результата, точную tail identity и temporary clear;
- полное освобождение nodes, head Strings, borrowed tails, frames и modules.

Native sink контролируется fixture; Sh service и GUI не исполняются.
16 malformed metadata случаев отказывают до allocation с неизменными owner/
REG. Проверены все 35/36 module-constructor allocation failures. Active
controls удаляют temporary clear и исходный tail transfer; каждый обязан
нарушить соответствующий runtime predicate.

Source chains16/32 проходят обычный parser и checked lowering. Граница
256 accepted /257 refused before allocation проверена на owning IR module:
столь длинная source expression раньше достигает существующего yacc stack
limit. Parser stack в этом изменении не меняется.

Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI, а также
native64-port.py. Локальное исполнение относится к amd64.

## Startup frontier

Неизменённый awm.command проходит old barrier PC1392 и Sh.run preparation.
Первый отказ теперь PC1401 ISEND — `sync <-= nil`. AWM init по-прежнему
требует admission полного spawned command. Wmsrv wm остаётся на typed-span
PC40 перед своим event loop. Полный AWM, Sh service и Android UI пока не
запущены; следующий этап включает channel communication и handler operands.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; allocations/refs отдельно проверяет fixture).
Регрессии: прежние String/List operations, checked spawn и полный emuinit/Arg.
Чистая amd64 console, paired boot и 324 настоящих C/JIT boot-процесса прошли.
Все 16 classic startup artifacts сохраняют обычные bytes.
