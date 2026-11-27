# Native64: checked local spawn

Checked LOAD lowering принимает ISPAWN с обычным local FRAME и fixed
nonallocating scalar/root argument stores. Допускается ранее проверенная
очистка compiler temporary после передачи root. Целевая функция обязана
быть void и целиком пройти checked lowering вместе со своими callees.
Recursive spawn, неподдерживаемое тело и imported MSPAWN остаются закрытыми.

Site использует существующую local-call geometry, contract0/inherited0 и
private end role186. Это не synchronous CALL и не определение результата
caller. Materializer сохраняет ISPAWN и связывает его target с private
Module program. Owning validator проверяет unique site owner, отсутствие
borrowed results у child, scalar/root maps у source и child arguments.
Orphan spawn, неправильный opcode/role, result binding и внешняя цель
отказывают до allocation.

Выполнение использует существующие production spawnready/spawnmodule,
newstack/newprog и unframe. Новый child stack получает mapped arguments;
источник передаёт владение после успешной публикации Prog. На ошибке
выделения stack/Prog/environment дочерний процесс не публикуется и pending
frame сохраняет исходные ссылки до unwind. Parent может завершиться до
child; module, String и Channel arguments сохраняют нужных владельцев.
Новые runtime structs или serialized bytecode formats не добавлены;
32-bit code paths и classic bytes сохранены.

## Проверки

`native64-spawn.py` компилирует семь полных Limbo bodies: один/два ребёнка,
relay с grandchild, временный String argument, child с globals и raise.
Независимый classic oracle обнаруживает 36 повреждений Spawn records и
сверяет FRAME, аргументы и точную цель ISPAWN. Boundary проверяет recursive
spawn, unsupported child body и imported mspawn.

1584 C/JIT runs с quanta1/3/1000 проверяют:

- FIFO и round-robin исполнение parent/child/grandchild в scheduler;
- Word, String и Channel arguments, nil и вычисленный temporary String;
- parent exit до выполнения детей и сохранность их module/data roots;
- ошибку ребёнка отдельно от родителя, production handler и exval cleanup;
- три allocation failures в первом/втором spawn, отсутствие частичного Prog;
- точное число исполненных детей и записанных ими scalar/root values;
- полное освобождение PID/run queues, stacks, environments, modules и roots;
- возврат shared Pgrp/Fgrp/Egrp references к исходным значениям.

16 metadata corruptions отказывают до allocation с неизменными owner/REG.
Все 24/25 module-constructor failures проверены на откат. Active controls
заменяют SPAWN на synchronous CALL и удаляют void-callee proof. Они обязаны
нарушить scheduler retirement либо preallocation predicate.

Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI, а также
native64-port.py. Локальное исполнение относится к amd64. Channel messaging
и полный GUI этим тестом не проверяются.

## Реальный startup frontier

Барьер теперь находится в телах spawned handlers. AWM init остаётся на
site PC127, но причина стала multi-callee: command (entry1388) пока закрыт
на external-list-init PC1392. Wmsrv.init остаётся на spawn site PC30:
wm (entry40) пока имеет typed-span на первом IMOVW.
Полный AWM init, Wmsrv event loop и Android UI ещё не запущены. Следующая
работа — lowering операций и аргументов внутри command/wm и обработчиков.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает refs и allocations).
Регрессии: mapped channels со своими controls, полный emuinit/Arg и 324
настоящих C/JIT boot-процесса. Чистая amd64 console и paired boot прошли;
16 classic startup artifacts совпадают с обычной компиляцией.

Продолжение: [borrowed List prefix](native64-list-prefix.ru.md) открывает
настоящую подготовку Sh.run arguments и переносит command с PC1392 на
ISEND PC1401. Admission полного command остаётся необходимым для AWM spawn.
