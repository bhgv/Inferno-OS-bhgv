# Limbo загружает и вызывает другой Limbo-модуль

В существующем коде Inferno загрузка и запуск — две отдельные операции:

- `appl/cmd/emuinit.b`: `load Command "/dis/wm/awm.dis"`, затем
  `mod->init(nil, args)`;
- `appl/cmd/sh/sh.b` и исходный `sh.y`: `load Command npath`, затем
  `mod->init(ctxt.drawcontext, argv)`;
- `externalexec` вызывает `mod->init(drawcontext, argv)` после подготовки
  окружения дочернего процесса.

`Command` — интерфейс из `module/sh.m`. AWM реализует `Wm` и экспортирует
совместимый `init`. C не должен отдельно вызывать `Wm.init`: он запускает
первый Limbo-модуль, после чего работают обычные LOAD/MFRAME/MCALL/RET.
Другие библиотеки могут предоставлять произвольные экспорты; имя `init`
не является автоматическим конструктором.

## Исправление ограниченного Dis64-профиля

Раньше producer и независимая проверка образа требовали, чтобы реализация
буквально называлась `Command`. Теперь имя реализации сохраняется в обычном
поле Module/Strings, а интерфейс запроса остаётся `Command`. Полные байты
сигнатуры должны совпасть. У настоящего `Wm.init` и вызывающего интерфейса
`Command.init` в текущих исходниках одинаковый классический хеш `0x4244b354`.

Сохраняются пределы проверенного тела, единственный публичный `init`, карты
аргументов и локальных корней, точные типы, запрет самостоятельного запуска
entryless-библиотеки и явный выбор экспериментального профиля. Дополнительный
публичный экспорт отказывает, а не теряется при записи файла. Полный исходник
AWM и общий модульный producer ещё требуют дальнейшей работы.

## Конечный пример

Это маленький проверяемый модуль с тем же интерфейсом, **не настоящий AWM**.
Сохраните `command.m`:

```limbo
include "sys.m";
include "draw.m";
include "sh.m";
```

Сохраните `wm-probe.b`:

```limbo
implement Wm;
include "command.m";
Wm: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };
init(ctxt: ref Draw->Context, argv: list of string)
{
    if(ctxt != nil || len argv != 2) { z := 0; bad := 1/z; }
    first := hd argv;
    if(first != "wm") { z := 0; bad := 1/z; }
}
```

Сохраните `boot.b`:

```limbo
implement Emuinit;
include "command.m";
Emuinit: module { init: fn(); };
init()
{
    args := "wm" :: "-probe" :: nil;
    mod := load Command "/probe-wm.dis64";
    if(mod == nil) { z := 0; bad := 1/z; }
    mod->init(nil, args);
    if(len args != 2) { z := 0; bad := 1/z; }
}
```

Компиляция остаётся явно выбранной:

```sh
limbo -64b -I module -I . -o probe-wm.dis64 wm-probe.b
limbo -64c -I module -I . -o boot.dis64 boot.b
emu-g -r /path/to/root -X dis64-calls -c 0 -d /boot.dis64
emu-g -r /path/to/root -X dis64-calls -c 1 -d /boot.dis64
```

Файлы должны находиться по указанным путям внутри выбранного VM root.
Нужен console build экспериментального профиля; обычный legacy startup не
переключается автоматически на Dis64.

## Проверки

`utils/tests/dis64-command-alias.py` проверяет реальные места вызова в
`appl/cmd`, сравнивает хеш настоящего AWM с интерфейсом вызывающей стороны и
компилирует конечные реализации `Command`, `Wm` и `Application`. Проверяются
прямой вызов, локальные помощники, вложенный импорт, context/argv, отдельный
ошибочный `init` и загрузка без его вызова. Пустые/повреждённые имена, другой
тип аргумента, другой экспорт и неверный хеш сохраняют свои отказы.

Дополнительные `--root`, `--emu` и `--runner` включают настоящий OS/scheduler
прогон двух Limbo-модулей, без C-вызова экспортируемого `init`. Нормальный
запуск по умолчанию и entryless-модуль остаются закрыты. Прежние `Command`
файлы и обычный 32-битный Dis сохраняют побайтовую совместимость.
