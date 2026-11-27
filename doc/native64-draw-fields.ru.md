# Draw payload roots и аргумент Image.display

Checked native64 compiler теперь переносит mapped-поля существующих
`Draw.Image`, `Draw.Screen`, `Draw.Display`. Полная nominal раскладка
проверяется до lowering: classic/native размеры 56/72, 16/32, 20/40,
выравнивание 8 и каждый scalar/root leaf. Original owner provenance
сохраняется и при повторном использовании classic scratch.

Поддержаны 11 полей: Image.display/screen/iname, Screen.image/fill/display,
Display.image/white/black/opaque/transparent. Владельцы и значения могут быть
в FP или MP; чтение, replacement, nil clearing и pointer nil branches
используют native offsets. String iname не сравнивается как ref с nil.
Scalar/Rect части Image не меняются этим путём.

Selected root fields Context/Window/Draw также могут служить аргументами
local/imported calls: MOVP читает поле напрямую в pending frame с точной
root map получателя. Отдельная аннотация разрешает compiler-generated
очистку временного root после передачи аргумента imported call. Проверяются
исходный temporary, предыдущая передача в mapped slot, readonly nil cell,
уникальный владеющий call и maps обеих сторон.

## Проверки

`utils/tests/native64-draw-fields.py` компилирует 24 Limbo функции и сверяет
каждый новый record с независимым classic disassembly oracle. Проверяются
666 повреждений records и неизменность classic bytes. Отдельно проверены
local field argument и отказы для foreign owner, чужого namespace и aggregate
field. Native размеры,
import signatures и prefixes получены реальным `limbo -H -M Draw`.

Owning runtime fixture выполняет 894 C/JIT случая: quanta 1/3/1000,
FP/MP owners, замены и очистки всех полей, nil owners/fields, возвращаемые
handles, выделение памяти с ошибкой, точные refcounts и полный retirement.
Два выражения выполняются из Limbo:

```limbo
return obj.display.color(123);
return Screen.allocate(obj, obj.display.color(123), 0);
```

Это controlled native Draw callbacks с настоящими generated prefixes
Display.color (80 bytes) и Screen.allocate (88 bytes). Перед allocate
проверяется, что временный fill уже очищен и остался один argument owner.
Настоящий Draw service, graphics connection и создание окна здесь не выполняются.

32 повреждения metadata отвергаются до allocation с неизменными owner/REG.
Все 64/65 точек allocation constructor в C/JIT проверяются на полный откат.
Active controls искажают field displacement, аргумент display и удаляют
семантику очистки temporary; каждый обязан вызвать смысловой отказ fixture.
Gate подключён к Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI.
Локальное исполнение относится к amd64.

## Настоящий AWM

Checked диагностика неизменённого `appl/wm/awm.b`: makescreen проходит первый
Display.color на PC1222. Следующий отказ PC1227 — external-inherit при
сохранении Screen.allocate result в глобальный screen. Первый отказ init
остаётся PC93, вызов ещё не допущенного makescreen. Все 16 classic startup
образов совпадают с ordinary compilation без diagnostics.

Полное окно AWM и Android UI ещё не запущены. 32-bit пути и classic wire
format сохраняются; новые annotations относятся только к checked native IR.

Локально gate прошёл UBSan и ASan/UBSan, включая три active controls
(LSan отключён; fixture считает собственные allocations). Чистая amd64
консоль, paired boot и 324 настоящих emuinit/Arg boot-процесса прошли.

Продолжение: [makescreen, global results и Image geometry](native64-makescreen.ru.md). Этот следующий gate расширяет описанный здесь checkpoint; makescreen теперь целиком допускается, следующий init barrier — Wmsrv.init на PC97.
