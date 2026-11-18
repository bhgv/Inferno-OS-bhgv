# Форматирование координат WM через checked native64

Checked compiler теперь принимает неизменённые `r2s` из `appl/wm/awm.b`
и `appl/lib/wmclient.b`. Первая преобразует четыре int координаты в строки
и складывает их; вторая сразу возвращает результат `Sys.sprint`.

Добавлены три связанные возможности:

* typed variadic tails для Int/Fix, Big, Real и String. Сохраняется существующий
  private ABI: каждый tail занимает 8 байт. Word/Fix записываются через CVTWL
  со знаковым расширением; Big/Real сохраняют все биты. Числа не являются GC roots;
* CVTWC и String concatenation непосредственно в caller-owned String result;
* явно отмеченный inherited result Sys call: FP[32] передаёт адрес результата
  из активного предка, без промежуточной строки или изменения числа инструкций.

У обычных функций int-поля и параметры остаются четырёхбайтными. Classic Dis,
его descriptors, установленный 32-битный ABI и обычное file admission не меняются.
Frame annotations и tail descriptors остаются доверенными compiler-owned C
данными; форматная строка не определяет типы аргументов. Opaque reference tails
не допускаются, предел — 256 tail slots.

Inherited режим по умолчанию выключен. Включённый режим проверяет полный caller
и callee frames, находит правильно выровненный result cell с нужной root map
в более раннем frame и подтверждает его по настоящей цепочке FP. Prepared,
но ещё не entered frames не становятся владельцами результата. Цепочка ограничена
64 переходами и текущим stack extent; перенос через другой extent здесь закрыт.
Адрес результата фиксируется после подготовки и повторно проверяется после
выделений formatter. Изменение сразу обеих копий адреса в caller/callee тоже
отклоняется. Сохраняются прежние Prog/epoch/Module/PC/Type/trampoline guards.

Проверка:

```
python3 utils/tests/native64-coordinate-format.py --limbo /path/to/host-tools/limbo
```

Gate использует неизменённые тела обоих WM helpers (переименованы только тестовые
экспорты) и две смешанные функции с локальным и перенаправленным результатом. Реальны scheduler/stack, C/JIT,
Sys typed formatter, parser и lib9 conversions. Доверенный native caller готовит
четыре скалярных поля Rect; это не реализация общего Limbo ABI передачи/возврата
Point/Rect и не загрузка целого wmclient/AWM.

Проверены 180 C/JIT вызовов с quanta 1/3/1000: предельные координаты int32,
Big min/max, Real, Unicode String, fresh обычный caller result и Sys forwarding.
Семь fault-сценариев покрывают scalar/misaligned/current-frame result targets,
разрыв цепочки, allocation failure, замену адресов после подготовки и замену
parent link после подготовки. Неправильный результат не публикуется; все owners
после теста освобождены.

Дополнительно проверяются 16 metadata refusals до allocation, все 40/41 allocation
failures конструктора C/JIT, 30 повреждений variadic reports, четыре opaque tail
refusals и 257-slot refusal после успешной обычной компиляции. Контрольные удаления
borrowed-result identity и знакового расширения должны провалить gate.

Локально gate прошёл UBSan и ASan/UBSan; LSan отключён из-за ptrace, allocations
fixture учитывает самостоятельно. Полная amd64-консоль, paired boot, прежние
324 real emuinit/Arg процессы, owning/formatter/getenv regressions проверяются
отдельно. CI включает gate для Linux amd64/arm64/riscv64 и Android ARM64 Bionic.

Текущий checked frontier: AWM 2/20, wmclient 5/21, titlebar 4/7, Arg 8/8,
emuinit 4/4. Classic bytes всех 16 startup-модулей совпадают. Полный AWM init
по-прежнему блокируют обращения к графическим ADT, aggregate calls и каналы/alt.
Окно AWM и запуск Android UI пока не выполнены.
