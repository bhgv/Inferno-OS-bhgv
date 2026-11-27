# Неизменённый emuinit в настоящем native64 boot

Добавлен отдельный выбранный режим `-X dis64-emuinit`. Полный исходный
`appl/cmd/emuinit.b` теперь исполняется в настоящем процессе Linux `emu`,
с настоящими Sys, OS namespace, formatter, stack/scheduler и файловым reader.
Это следующий этап после [owning source gate](native64-emuinit-full.ru.md).

Сборка и проверка:

```
python3 utils/tests/native64-port.py --arch amd64 \
  --build-root /path/to/empty-build --host-tools /path/to/host-tools \
  --pair-boot-only --emuinit-boot
/path/to/empty-build/emu/Linux/o.emu-g -r /path/to/root -v -X dis64-emuinit -c 1
```

В обычной configure-сборке нужны `--console --native64-scalar-boot
--native64-sys-io-boot --native64-emuinit-boot` и соответствующая 64-битная
Linux architecture. Последняя опция включает coherent typed formatter и
build-time generated `emuinit-native64.h`; host Limbo проверяет все четыре
функции неизменённого source и classic-byte equality. Runtime не читает
непроверенный emuinit.dis. Режим несовместим с `-d` и pair/trial options.
Без этой явной сборочной опции обычный boot не меняется.

Начальный Module передаётся обычным module list и schedmod: после публикации
в списке внешний constructor GC pin снимается, единственная Module reference
переходит scheduler. Обычные unload/freemod освобождают данные и program sites.
Различены C-имена builtin registration request (`Native64BuiltinSpec`) и
source module request (`Native64ModuleSpec`), которые раньше конфликтовали
при включении обоих owning constructors в runt.c. Wire ABI не изменён.

Для этого trusted caller создаётся отдельный Sys view. Fixed entries получают
проверенные hash/frame/map и реальные функции из выбранного Sys registry.
Только два variadic entries используют program-owned trampoline. Общий Sys
registry и обычное file admission не расширяются.

Command LOAD по `/dis/wm/awm.dis` и затем `/dis/sh.dis` использует существующий
namespace/file reader с реальными kopen/kread/kclose, signing policy и checked
structural Command.init admission. Тип и canonical consistency hash запроса
получены отдельным существующим `-64c` producer. Classic-файл или несовпадающий
canonical hash не допускаются. Непроверенные файлы не исполняются.

Gate `native64-emuinit-boot.py` проверяет 18 целых процессов C/JIT:

* оба Command отсутствуют: реальные sprint/fprint и terminal fail:error;
* checked file Command по пути AWM и shell fallback, nil Context и argv из Limbo;
* непустой #e/emuargs через настоящие bind/open/fstat/read и unquoted,
  включая пробелы, апострофы и Unicode;
* неверное тело provider действительно вызывает zero divide (положительный
  результат не получен обходом вызова);
* classic-file и canonical-hash refusals на обоих путях;
* штатное завершение scheduler и отказ несовместимым CLI options.

Теперь выбранный boot включает [настоящий неизменённый Arg](native64-arg.ru.md).
Он разбирает исходный emuargs и передаёт Command оставшиеся аргументы;
пустой остаток приводит к default `-l`. Host-only `-X dis64-emuinit` исключён
из emuargs по фактически разобранному параметру, reboot argv сохранён.
Расширенный gate проверяет 324 процесса с тремя способами выбора режима и
шестью вариантами argv. Приведённые выше 18 путей — предыдущий этап;
AWM по-прежнему заменён checked provider в тесте.

Локально проверен Linux amd64. CI paired-console теперь включает этот режим
и gate на amd64/arm64/riscv64. Это не Android runtime validation.

Сам AWM в gate заменён небольшим source-compiled checked Command provider.
Настоящий awm.b пока принимается лишь частично; Draw/Tk и графический Android
host остаются следующей границей. Окна AWM ещё нет. 32-битные startup пути,
classic bytecode и их установленный ABI не изменены.
