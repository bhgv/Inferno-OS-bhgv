# Native64: завершение Limbo-процесса

Checked LOAD профиль допускает обычный Limbo `exit` / IEXIT с пустыми
operands. Это завершение текущего процесса через существующий VM handler,
а не возврат из функции: вложенные кадры и pending roots освобождаются
планировщиком. Другие процессы продолжают работу. Реализация VM/JIT и
32-битные пути не заменяются; classic bytecode остаётся прежним.

Компилятор рассматривает IEXIT как терминал без ребра к следующей инструкции.
Поэтому функция с результатом может закончить процесс, не записав borrowed
result. Обычный RET по-прежнему требует определённого результата. Старые
профили без checked LOAD сохраняют отказ для этого исходного кода.
Материализатор проверяет пустые operands/role, а owning builder отклоняет
повреждённые терминалы и IEXIT внутри call island до allocation.

`native64-process-exit.py` проверяет 12 полных Limbo bodies независимым
classic oracle и обнаруживает 30 повреждений exit records. Отдельно проходит
весь неизменённый awm.command с точной проверкой инструкций, relocation,
дескрипторов и imported calls; обнаруживаются все 462 одиночных
искажения шести полей его 77 инструкций. В реальном коде повторно используемый mapped
временный слот может иметь исходный list descriptor; oracle допускает его
как String-путь LOAD только непосредственно после проверенного IADDC
с двумя String operands, записавшего этот самый слот.

648 C/JIT запусков проверяют:

- прямой exit и exit из вложенного локального вызова;
- FIFO/round-robin, quanta 1/3/1000, child и grandchild isolation;
- отсутствие продолжения вызывающего кода после exit;
- сохранность прежнего scalar/root результата, если функция завершила процесс;
- обычный возврат на соседней ветке и точные счётчики выполненных тел;
- nil и временные String roots, каналы, соседние Word bytes и caller guards;
- полное освобождение кадров, аргументов, environments, modules и shared refs.

26 malformed terminal/call-island случаев отклоняются до allocation.
Все 33/34 allocation failures конструктора C/JIT полностью откатываются.
Два active controls подменяют EXIT на RET и убирают раннюю проверку терминала;
оба должны дать смысловой predicate failure без sanitizer crash.

Gate включён в Linux amd64/arm64/riscv64, Android ARM64 Bionic workflow
и native64-port.py. Локальное исполнение и sanitizers относятся к amd64;
межархитектурный CI здесь не объявляется выполненным.

## Startup frontier

Настоящий awm.command принимается целиком. AWM init проходит spawn setup
на PC127/ISPAWN PC131 и останавливается на PC136 с multi-argument:
вычисление `"cannot run command: " + e` прямо в аргументе локального fatal.
Wmsrv.init остаётся на PC30 (callee wm), а wm — на typed-span PC40.
Это compiler admission, не запуск настоящего графического AWM/Sh сервиса.
Следующий участок — computed String argument локального вызова и затем
оставшиеся операции event loop/ALT.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; ref/allocation retirement проверяет fixture).
Регрессии: spawn (1584), int lists (1584), полный emuinit/Arg (48),
чистая amd64 console и paired boot, 324 настоящих C/JIT загрузки Arg/emuinit.
Все 16 classic startup artifacts сохраняют обычные bytes.
