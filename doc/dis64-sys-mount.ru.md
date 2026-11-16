# Mount Styx из Limbo на native64

Выбранный профиль Dis64 теперь выполняет обычные вызовы `Sys.dial`, `Sys.mount`, `Sys.open`, `Sys.read`, `Sys.write` и `Sys.unmount`. Ниже — клиент тестового сервера libstyx, который публикует файл `first` по адресу `tcp!127.0.0.1!5640`. `/mnt` должен существовать в корне emu. Сервер должен разрешать подключение пользователя emu; это не пример аутентификации.

Сохраните как `mountdemo.b`.

```limbo
implement SysText;
include "sys.m";
SysText: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    (status, c) := sys->dial("tcp!127.0.0.1!5640", nil);
    if(status < 0) return;
    r := sys->mount(c.dfd, nil, "/mnt", Sys->MREPL, "");
    if(r < 0) return;
    fd := sys->open("/mnt/first", Sys->OREAD);
    buffer := array[64] of byte;
    r = sys->read(fd, buffer, len buffer);
    if(r < 0) return;
    text := string buffer[:r];
    r = sys->werrstr(text);
    r = sys->unmount(nil, "/mnt");
}
```

```sh
limbo -64l -I module -o mountdemo.dis64 mountdemo.b
emu-g -r /path/to/root -c 1 -d /mountdemo.dis64 -P '$Sys' -X dis64-managed-trial
```

Положите результат в выбранный корень emu. `-c 0` выбирает C-интерпретатор. Читается только фактически полученное число байтов; `text` сохраняется в буфере ошибки через `werrstr` для диагностики этого примера. Это не печать в консоль. Для общего клиента нужны повторные read до EOF и обработка ошибок с очисткой; циклы и такие обработчики ещё не входят в выбранный профиль.

`load Sys Sys->PATH` компилируется в LOAD с путём `$Sys`. Хэш интерфейса и C-кадр берутся из `sys.m` и генератора Limbo. Для mount передаются FD транспорта, nil вместо auth-FD, путь, flags и spec. Wire-протокол остаётся base 9P2000. Native-указатели и ADT не передаются по сети.

После `unmount` уже открытый FD продолжает владеть каналом; он закрывается при замене или завершении программы. В текущем профиле допускается один отдельный результат open, в том числе его замена, вместе с полями Connection. Корень FD отслеживается GC отдельно от числовых результатов. Доступны линейные вызовы и ранний return, до 32 вызовов и 8 выбранных методов Sys. Несколько независимых FD, произвольные ADT/алиасы, циклы и общий sh/AWM остаются отдельной работой.

Туториал компилируется и выполняется в C/JIT на x64 и ARM64/QEMU через настоящий TCP, включая фрагментированный ответ версии. Это подтверждение гостевого mount в emu, не Android GUI/APK.
