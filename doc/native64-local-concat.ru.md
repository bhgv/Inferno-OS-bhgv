# Native64: вычисляемый String аргумент локального вызова

Checked LOAD профиль допускает `callee(left + right)` для полностью
проверенного локального callee с одним аргументом типа string. Это участок
`fatal("cannot run command: " + e)` в настоящем awm.init. Создание строки
идёт через существующий addc прямо в mapped pending frame; C/JIT сохраняют
порядок operands, ссылки и обычный borrowed-result ABI.

Компилятор требует ровно один String formal для нового случая; allocating
spawn arguments остаются закрытыми. Loader и materializer требуют один
computed аргумент в call island, destination64, mapped source/middle и
callee root slot, точные frame bounds и result binding. Старый self-tail
concat и imported-call concat сохраняют свои правила.

`native64-local-concat.py` строит 13 полных Limbo bodies и сверяет обычные
opcode/operands, frame descriptors, local targets и result contracts.
Независимый oracle обнаруживает 48 повреждений computed argument records.
Отдельно отвергаются allocating spawn и два formal arguments.

624 C/JIT сценария с quanta1/3/1000 проверяют:

- literal+FP, FP+literal, FP+FP и literal+MP;
- nil, пустые и ASCII строки, Rune и embedded NUL;
- fresh/inherited String result, void callee и дополнительный вложенный вызов;
- точный текст, тождество возвращённого аргумента и число вызовов;
- отказ выделения при addc, raise и exit после публикации аргумента;
- сохранность прежнего результата и отсутствие caller continuation при exit;
- соседние Word bytes и полное освобождение pending roots, exceptions,
  кадров, modules, environments и shared refs.

12 malformed metadata случаев отказывают до allocation. Все 35/36 точек
отказа конструктора C/JIT откатываются. Два active controls смещают middle
operand в материализованном addc и удаляют проверку callee root map;
оба должны дать semantic predicate failure без sanitizer crash.

Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI и общий
native64-port.py. Локальное исполнение относится к amd64; CI для других
архитектур здесь не объявляется выполненным. Classic bytes и существующий
32-битный код не меняются.

## Startup frontier

Весь настоящий awm.command остаётся accepted. AWM init проходит локальный
fatal на PC136 и доходит до PC140 IMOVW (typed-span): чтение win.ctl при
подготовке ALT event loop. Wmsrv.init остаётся на PC30, callee wm — PC40.
Полный графический AWM, event loop и Android UI ещё не запущены.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; ref/allocation retirement проверяет fixture).
Регрессии: imported concat (2016), process exit (648), полный emuinit/Arg
(48), чистая amd64 console/paired boot и 324 настоящих C/JIT boot-процесса.
Все 16 classic startup artifacts сохраняют обычные bytes.
