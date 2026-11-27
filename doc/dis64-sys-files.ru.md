# Создание и удаление файлов через native64 Styx

Выбранный профиль Limbo соединяет `dial/mount/create/open/read/write/remove/chdir/bind/unmount` через существующие native-реализации Sys. Для создания нужен `MCREATE` у mount и право записи в корень сервера. Пример работает с синтетическими данными отдельного соединения из `tools/libstyx/tests/loopback.c`, без записи файлов хоста. Подготовьте `/mnt` и `/alternate` в корне emu и разрешите имя пользователя emu в тестовом сервере.

```limbo
implement SysText;
include "sys.m";
SysText: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    (status, c) := sys->dial("tcp!127.0.0.1!5640", nil);
    if(status < 0) return;
    r := sys->mount(c.dfd, nil, "/mnt", Sys->MCREATE, "");
    if(r < 0) return;
    r = sys->chdir("/mnt");
    fd := sys->create("created", Sys->ORDWR, 8r640);
    buffer := array of byte "created-data";
    r = sys->write(fd, buffer, len buffer);
    r = sys->bind("/mnt", "/alternate", Sys->MREPL);
    fd = sys->open("/alternate/created", Sys->OREAD);
    r = sys->remove("created");
    buffer = array[32] of byte;
    r = sys->read(fd, buffer, len buffer);
    if(r < 0) return;
    text := string buffer[:r];
    r = sys->werrstr(text);
    r = sys->chdir("/");
    r = sys->unmount(nil, "/alternate");
    r = sys->unmount(nil, "/mnt");
}
```

Компиляция: `limbo -64l -I module -o files.dis64 files.b`. Поместите результат внутрь корня emu и запустите `emu-g -r /path/to/root -c 1 -d /files.dis64 -P '$Sys' -X dis64-managed-trial`. Для C-интерпретатора используйте `-c 0`. Вместо5640 укажите порт тестового сервера.

`chdir` разрешает относительные пути в существующем namespace. `bind` делает тот же смонтированный каталог доступным по другому пути. Замена `fd` освобождает прежнего владельца FD. Удаление имени запрещает последующий walk, но уже открытый канал продолжает читать `created-data`. `werrstr` сохраняет этот текст в буфере ошибки, а не печатает его.

Native create получает mode на смещении72, perm —76 в сгенерированном C-кадре размером80байт. Результат FD использует отдельный корень caller-кадра с GC-картой, как open; числовые результаты с ним не пересекаются. Частное дерево сигнатуры берётся из существующего интерфейса Sys. Новых публичных wire kinds и форматов сообщений Styx нет.

Это ограниченный проверочный пример: один отдельный результат FD с заменой, линейные вызовы и ранний выход по проверке, максимум32 вызова и12 выбранных методов Sys. Полная обработка ошибок, циклы чтения и независимые FD-алиасы пока не поддержаны этим профилем. Пример проверяется на x64 и ARM64/QEMU в C/JIT по TCP, включая фрагментированный ответ версии. Запуск Android GUI/AWM ещё не подтверждён.
