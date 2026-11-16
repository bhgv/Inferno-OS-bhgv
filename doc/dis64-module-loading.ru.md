# Загрузка модулей и явный вызов init

Команды и библиотеки Inferno устроены как одни и те же модули. Команда Limbo
`load` превращается в Dis LOAD: runtime загружает модуль и связывает запрошенный
интерфейс. Экспорт `init` при этом не вызывается. Отдельный вызов делает shell.
В `appl/cmd/sh/sh.b` это видно в последовательности `load Command` и следующего
`mod->init(ctxt.drawcontext, argv)`; исходный генератор — `sh.y`.

В данной ветке есть и более прямой путь: `appl/cmd/emuinit.b` загружает
`/dis/wm/awm.dis` через интерфейс `Command`, затем сам вызывает
`mod->init(nil, args)`. Реализация AWM называется `Wm`. Имя реализации не
обязано совпадать с именем интерфейса: связываются экспорт `init` и его полный
типовой хеш. C-часть запускает первый Limbo-модуль через `disinit/schedmod`;
дальнейший запуск команды выполняет Limbo-код. Для библиотек `init` вообще не
обязателен: вызывается нужный экспорт запрошенного интерфейса.

Ограниченный Dis64-профиль `Command.init` теперь сохраняет настоящее имя
реализации и допускает, например, `load Command` для модуля `Wm`. Проверки
интерфейса, полного хеша, экспортов и карт остаются. Это ещё не допускает
полный исходник AWM; [проверяемый пример](dis64-command-alias.ru.md) отдельно
воспроизводит цепочку Limbo-загрузчика.

`Sys` — описатель интерфейса, `$Sys` — внутренний путь зарегистрированного
builtin-модуля. Native-экспорты разрешаются в C-функции, Dis-экспорты — в PC
байткода, через существующие таблицы экспортов и linker. Новая строковая группа
использует те же C-функции Sys и сгенерированные `limbo -H` кадры.
C-регистрация `sysmodinit` при старте host отделена от вызова Limbo-экспорта
с именем `init`. Основой вызовов остаётся существующий код владения и планировщика.

В черновых метаданных Dis64 сейчас различается наличие bootstrap entry.
Это подсказка явному консольному запуску, а не отдельный ABI библиотеки и не
приказ LOAD вызвать `init`. Ограниченный producer теперь сначала проверяет
полные тела экспортов и лишь затем рассматривает необязательную entry-подсказку
компилятора. Экспорт с именем `init` допускается как обычная вызываемая функция,
если его тип и тело укладываются в поддержанный профиль.

В рабочем примере вызов сделан явно. Сохраните `initlib.m`:

```limbo
InitLib: module {
    init: fn(value: string): list of string;
    unpack: fn(values: list of string): string;
};
```

Сохраните `initlib.b`:

```limbo
implement InitLib;
include "initlib.m";
init(value: string): list of string { return value :: nil; }
unpack(values: list of string): string { return hd values; }
```

Сохраните `caller.b`:

```limbo
implement Caller;
include "initlib.m";
Caller: module { init: fn(); };
init()
{
    lib := load InitLib "/initlib.dis64";
    values := lib->init("explicit call");
    text := lib->unpack(values);
}
```

```sh
/host/tools/limbo -I . -64m -o /build/initlib.dis64 initlib.b
/host/tools/limbo -I . -64l -o /build/caller.dis64 caller.b
/build/emu/Linux/o.emu-g -v -c 0 -r /build \
    -d /caller.dis64 -P /initlib.dis64 -X dis64-managed-trial
```

Для запроса JIT повторите с `-c 1`. Пример завершается без печати.
Тип `InitLib->init` задаётся собственным `.m`; это не интерфейс `Command`,
которым пользуется shell. Проверка с настоящим GC удостоверяется, что первый
LOAD ещё не создал List и не установил bootstrap entry provider до явного вызова.
`dis64-managed-module.py` компилирует и запускает этот случай вместе с другими
произвольными именами экспортов.

Исправление касается выбора роли внутри существующего ограниченного
String/List-producer. Полная сигнатура `Command.init(Draw.Context, list of string)`
и запуск Limbo-shell/AWM на Dis64 пока не допущены. Для этого ещё нужны общие
типы данных и сигнатуры вызовов.
