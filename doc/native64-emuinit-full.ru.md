# Полный исходный emuinit в checked native64

Checked LOAD compiler принимает все четыре функции неизменённого
`appl/cmd/emuinit.b`: `init`, `loadmod`, `getenv`, `unquoted`.
Private variadic lowering связывает исходные FRAME/MCALL с instantiated
типом аргументов: Sys.sprint и Sys.fprint, String tails, fresh результат.
Поддержаны три существующих site с native frames 72/88/96 байт.
Обычные external/file profiles variadic-вызовы по-прежнему не принимают.

Compiler сохраняет per-use Type на Addr и instantiated Type на FRAME только
в IR. Переиспользуемый managed `.t*` temporary получает уточнение номинального
класса лишь после проверки исходного root span и лишь в LOAD profile.
Wire instructions, classic descriptors и 32-битный ABI не меняются.
Независимый classic decoder проверяет layouts, operand relocations, CASE,
все import tables, exports и каждый variadic site. Classic bytes совпадают
между обычной компиляцией и checked LOAD report.

`native64-sys-module.h` — явный trusted owning adapter, без нового file ABI.
Он проверяет карты, slots, result contracts и отсутствие переходов внутрь
setup; строит базовый module с inert placeholders, устанавливает полные
call-site Types и program-owned binder/trampoline, затем создаёт optional JIT.
Импортированный Sys entry может иметь точный generated fixed-prefix Type;
полный frame и его String tails принадлежат FRAME site. Trampoline identity
остаётся обязательной. Выход публикуется после успешной сборки, REG сохраняется.
Stack hint резервирует путь distinct functions плюс наибольший native callee;
произвольная рекурсия этим gate не проверяется.

Проверка:

```
python3 utils/tests/native64-emuinit-full.py --limbo /path/to/host-tools/limbo
```

Gate исполняет настоящий полный Limbo init/loadmod в scheduler/stack runtime
и native backend. В 48 прогонах C/JIT, с quanta 1/3/1000, проверяются:

* Arg отсутствует либо доступен; opt/CASE/arg и default/supplied argv;
* первым Command LOAD всегда является `/dis/wm/awm.dis`;
* `/dis/sh.dis` используется только после неудачи первого LOAD;
* точные default `-l` и fallback `sh -c $* ...` аргументы;
* `mod->init(nil,args)` вызывается инструкциями Limbo;
* terminal `raise "fail:error"`, штатный exception handler и полный teardown.

LOAD, OS effects, Arg и Command.init здесь управляемые; fprint/sprint producer
проверяет точные аргументы, но возвращает контролируемые результаты. Настоящие
formatter/GC и непустой getenv с C-телами Sys.open/fstat/read проверяются
отдельными существующими gates. Этот тест не загружает реальный awm.dis,
не создаёт окно и не означает запуск на Android устройстве.

Также проверяются 19 metadata refusals до allocation, все allocation failures
конструктора C/JIT (72/73), 27 повреждений variadic reports, raw IR refusals
и активные удаления проверок сигнатуры, LOAD-only refinement и frame maps.
Локально gate прошёл UBSan и ASan/UBSan (LSan недоступен под ptrace;
все runtime allocations отдельно учитываются fixture). Полная amd64-консоль
и paired boot также прошли. Checked preflight всех 16 startup-модулей
подтвердил неизменность classic bytes; у AWM пока принято 1/20 функций.
CI включает gate для Linux amd64/arm64/riscv64 и Android ARM64 Bionic.

Следующая граница — применение того же owning пути в выбранном boot profile
и загрузка настоящих зависимостей AWM/Draw/Tk; затем графический Android host.

Следующий этап: [настоящий emuinit boot](native64-emuinit-boot.ru.md), реальные Sys и Command file reader, 18 целых C/JIT процессов. Реальный AWM остаётся следующим этапом.

Следующий этап: [координаты WM и числовые variadic tails](native64-coordinate-format.ru.md). Старый all-String режим сохранён; новый private compiler/owning путь добавляет явные scalar slot kinds и inherited results.
