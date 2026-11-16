# Command с собственной зависимостью

Теперь выбранный профиль `-64b` поддерживает Command, который экспортирует обычный `init` и сам делает `load` одного файлового модуля. `load` связывает модуль; `init` вызывается явно.

Сохраните блоки по порядку как `library.m`, `dependency.b`, `command.b`, `client.b`. Интерфейс Command берётся из `module/sh.m`; хэш вычисляет компилятор.

```limbo
Library: module {
    inc: fn(n: int): int;
};
```

```limbo
implement Library;
include "library.m";
inc(n: int): int
{
    return n + 1;
}
```

```limbo
implement Command;
include "sys.m";
include "draw.m";
include "sh.m";
include "library.m";
init(ctxt: ref Draw->Context, argv: list of string)
{
    if(ctxt != nil || len argv != 2){ z := 0; bad := 1/z; }
    m := load Library "dependency.dis64";
    if(m == nil){ z := 0; bad := 1/z; }
    n := len argv;
    value := m->inc(n);
    if(value != 3){ z := 0; bad := 1/z; }
}
```

```limbo
implement Client;
include "sys.m";
include "draw.m";
include "sh.m";
Client: module { init: fn(); };
init()
{
    m := load Command "library.dis64";
    if(m == nil){ z := 0; bad := 1/z; }
    argv := "wm" :: "abc" :: nil;
    m->init(nil, argv);
    m->init(nil, argv);
    if(len argv != 2){ z := 0; bad := 1/z; }
}
```

```sh
limbo -64b -I module -I . -o dependency.dis64 dependency.b
limbo -64b -I module -I . -o library.dis64 command.b
limbo -64c -I module -I . -o client.dis64 client.b
emu-g -r . -c 1 -X dis64-calls -d /client.dis64
```

Команды рассчитаны на подготовленный 64-битный emu с поддержкой Dis64; `-r` должен указывать корень, содержащий три результата. Для C-интерпретатора используйте `-c 0`. Здесь клиент вызывает Command дважды с двумя аргументами; Command проверяет Context и список, затем вызывает `Library.inc(2)` и проверяет результат 3. Отсутствующая зависимость приводит к намеренной ошибке проверки.

Библиотека не имеет точки входа процесса. Её RET возвращает управление клиенту. Исходящие Imports и входящий Exports — разные таблицы существующих секций Dis64. Буферы запросов и имён принадлежат загруженному модулю.

Ограничения этой ступени: один путь/группа импортов, скалярные сигнатуры зависимости либо фиксированный Command.init, один публичный init. Частные помощники поддерживаются, но импортировать может только корневой init. Общие ADT, произвольные публичные сигнатуры, несколько путей, обработчики исключений и изменяемые глобальные MP ещё не покрыты. Это не подтверждение запуска Android GUI/AWM.
