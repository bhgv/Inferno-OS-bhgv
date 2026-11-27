# Настоящий Arg в native64 startup

Checked LOAD compiler принимает все 8 функций неизменённого
`appl/lib/arg.b`. Добавлены строковые сравнения и ограниченная запись
`sliceC` в mapped String module data. Сохраняются проверки nominal Type,
полных native pointer spans, root maps и локальных branch targets.
Другие external profiles не получают запись String-среза в MP.

String branches включены в анализ определения результата: переход не может
обойти запись в caller-owned result и привести к RET с неопределённым
результатом. Raw IR gate проверяет все шесть сравнений, обе определяющие
ветви и отказ scalar middle; удаление CFG predicate обязано провалить gate.
Trusted materializer отдельно проверяет native operand geometry и root maps;
контрольное удаление проверки карты тоже обнаруживается тестом.

Build-time generator выбранного emuinit boot включает полный Arg source module
с private fprint site в usage. `/dis/lib/arg.dis` в этом trusted startup
разрешается в owning module из проверенных исходников. Произвольные файловые
Arg providers этим не допускаются. Sys view для Arg использует тот же проверенный
fixed/prefix adapter, что и emuinit. После LOAD обычные Limbo MCALL вызывают
Arg.init/opt/arg/argv; native C-замен этих функций нет.

Host-only выбор `-X dis64-emuinit` убран из #e/emuargs по индексам реально
разобранного параметра. Поддержаны отдельная и слитная формы. Отдельно
сохранённый reboot argv остаётся исходным. Совпадающая строка после `--`
сохраняется пользовательским аргументом. Через `EMU` профиль выбирается без
добавления служебных слов в исходный argv. Повторный выбор профиля отклоняется.

Реальный boot gate выполняет 324 C/JIT процесса: три способа выбора профиля,
шесть argv cases, девять путей загрузки/ошибки. Проверяются default `-l`,
пробелы/апострофы/Unicode, attached/separate/grouped flags, `--`, AWM-first
Command LOAD, shell fallback, фактический вызов файлового Command.init,
classic-file/hash refusals и terminal errors. При этом используются настоящие
Sys bind/open/fstat/read, formatter, namespace reader и scheduler.

Дополнительно проходят полная сборка amd64, paired boot и owning gates:

* полный Arg: все 48/49 allocation failures C/JIT, неизменность REG/output,
  освобождение всех partial owners;
* пять отказов String branch/slice metadata до allocation;
* 9 повреждений variadic report Arg и независимый classic operand/layout oracle;
* прежние 48 source-owned emuinit launches и 72/73 constructor failures;
* UBSan и ASan/UBSan; runtime fixture учитывает allocations отдельно от LSan.

В реальном startup исполняются четыре используемых метода Arg. `earg`,
`setusage`, `progname`, `usage` проходят полную компиляцию и owning construction;
этот boot gate не заявляет отдельное исполнение всех их вариантов.

Classic bytes проверены для всего startup slice, включая Arg. 32-битный ABI
не менялся. Локальный runtime — Linux amd64; CI тот же gate запускает на
amd64/arm64/riscv64. Настоящие AWM/Draw/Tk и Android window остаются следующими
границами. AWM-окно пока не запущено.
