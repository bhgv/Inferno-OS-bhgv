# Native64: геометрия и ввод Wmclient.Window

Полный исходник `appl/lib/wmclient.b` принимает 9/21 тел вместо 7/21.
К ранее исполняемым `Window.screenr` и `Window.imager` добавились
неизменённые `Window.pointer` и частный `sizetb`.

## Изменения

Сохраняется полная проверка nominal Wmclient.Window: 104 classic байта,
128 native байт, 26 листьев, шесть roots, GC-карта 92/c4. Все 20 Word32
полей читаются, записываются и передаются в проверенные CALL/MCALL.
Скалярные Image.depth/chans.desc/repl используют точные смещения 32/36/40.

Теперь копируются целые Point/Rect из/в Window и обычные сырые FP/MP
слоты. Разрешены три Rect (r, displayr, tbrect) и восемь Point (их min/max,
tbsize, saved). Аннотации 82/83 несут отдельно размер и native смещение;
проверяются полный владелец, границы, карты roots/raw слотов и отсутствие
пересечения с call-site. Heap-to-heap копии остаются закрыты.

Аннотация 84 передаёт геометрию Window в аргумент локального/внешнего
вызова. Проверяются единственный call-site, карта владельца, сырая роль
каждого байта аргумента и границы кадра. Канонический Draw.Pointer также
можно передать по значению как четыре непрерывных Word32, сохраняя его
nominal отличие от Rect. Это открывает исходный Window.pointer.

Аннотация 81 разрешает Point/Rect результат внешнего или локального
вызова прямо в поле Window. Compiler требует именованный формальный
аргумент ref Wmclient.Window и совпадение его classic слота; полный layout
определяет native смещение. Module builder требует единственную свежую
result-binding site, точный контракт 96/112 и mapped owner root.
Runtime перед выдачей адреса проверяет live owner, Type размером 128,
карту 92/c4, допустимый диапазон поля и кадры caller/callee.
Глобальные владельцы здесь закрыты: их можно заменить при приостановке
callee. Полученный адрес удерживается корнем аргумента вызывающего кадра.

Отдельный callerresultcontract по-прежнему обязателен для inherited
внешнего результата. Локальные inherited geometry-вызовы закрыты.
Обычный Dis и 32-битные пути не меняются.

## Проверка

`native64-wmclient-geometry.py` выполняет 3432 C/JIT вызова с квантами
1/3/1000. Fixture включает четыре неизменённых исходных тела и 87
проверенных тел суммарно; канонические import frames получены из Draw/Tk
headers. Draw Rect.dx/dy/inset/size и Point.add/in взяты из производственного
`libinterp/geom.c`. Tk.rect/Tk.pointer — контролируемые сервисы, проверяющие
точные owner, widget, flags и байты Pointer; это не полноценный Tk backend.

Проверяются все 11 геометрических полей, крайние Word32, сохранность
соседей и roots, независимые результаты caller и поля Window, прямые и
вложенные local/external результаты. Матрица исходного pointer охватывает
screen/titlebar, три состояния фокуса, старые/новые кнопки, точки внутри и
на исключённой границе прямоугольника. sizetb проверяется с/без titlebar.
Nil-пути всех новых операций и полное освобождение включены.

Независимый source oracle обнаруживает 486 искажений, одновременно
доказывая идентичность classic bytes. Runtime отвергает 569 вариантов
метаданных, включая 81 owner-map control, до выделения памяти.
Конструктор проходит все 205/206 точек отказа C/JIT с атомарным rollback.
Три активных контроля подменяют scalar offset, geometry result field и
удаляют caller contract proof. Регрессионная граница Window-fields теперь
проверяет foreign geometry owner вместо уже разрешённого canonical Rect.

На amd64 выполнены UBSan и ASan/UBSan (detect_leaks=0; внутренний полный
retirement включён), шесть регрессий geometry/Window/Wmsrv/AWM и чистая
сборка console с pair/emuinit boot. Существующие native64-port и оба
задания native64-typed-calls CI запускают расширенный тест.

## Следующие ограничения

Это ещё не настоящее окно AWM и не запуск Android GUI. Полный Wmsrv
сохраняет 34/34 тел. Window.reshape проходит запись результата screenr в
Window, но останавливается на следующем multi-callee (wmctl). Дальше нужны
клиентский обмен wmctl/wmreq, tuple результаты, cursor/image аргументы и
сборка полного оконного пути. Локальная проверка этого блока — amd64;
результаты arm64/riscv64 CI следует проверять отдельно.

Следующий блок и актуальные расширенные проверки описаны в
[native64-wmclient-tuples.ru.md](native64-wmclient-tuples.ru.md).
