# Native64: создание Draw.Context и копирование полей

Checked compiler теперь допускает выделение `Draw.Context`: classic12 ->
native24, три mapped pointer cells по offsets0/8/16. Проверяются nominal
Draw.Context, исходный descriptor12/map E0, полная native layout/alignment
и классы полей Ref/Ref/Chan. Другие ADT и чужой namespace остаются закрытыми.

## Allocation и copy

Обычный INEW/INEWZ создаёт объект в mapped FP slot. Отдельная private
annotation допускает выделение сразу в caller result cell — так компилятор
представляет `return ref Context`. Этот путь определяет результат в CFG
analysis и использует уже проверенный восьмибайтный result address.
При ошибке выделения старое значение сохраняется.

Owning constructor создаёт отдельный Type24/E0 для Context и переписывает
его operand ordinal. Byte-array Type и mapped triple-copy Type сохраняются
отдельно: положительный тест и активный контроль проверяют все три ordinals.
Не добавлены общий ADT allocator, MP allocation и создание внутри call setup.

Кроме прежних transfers field<->direct root добавлено копирование одного
поля Context прямо в такое же поле другого Context. Оба original owners,
их полные layouts, field index и FP/MP root maps проверяются до lowering.
Source/destination displacement материализуется отдельно; MOVP сохраняет
нужную ссылку до освобождения старой, включая self copy. Разные ADT,
разные индексы полей и вложенные dereference chains не добавлены.

## Проверки

`utils/tests/native64-context-new.py` компилирует 13 Limbo функций:
конструктор с аргументами, пустой Context, clone, повторное выделение,
результат в global через local call, FP/MP field copies, self copy.
Дополнительные функции требуют byte-array и channel-triple copy descriptors
в том же модуле. Draw layout проверяется через настоящий `limbo -H` header.

318 C/JIT случаев с quanta1/3/1000 включают:

- заполненные/nil/alias поля, raw allocation и отдельный INEWZ variant;
- FP->FP, MP->MP, FP->MP, MP->FP и self field copies;
- nil source/destination, ошибки первого/второго Context allocation;
- старый caller/global result, точные child refs и полное освобождение всех
  objects, frames, module instances и descriptors.

Используются production newrecord/NEW/NEWZ и heapz в учитывающем allocations
fixture; обёртка heap эквивалентна для полностью mapped Context. Поля содержат opaque managed handles;
Draw connection и channel communication не выполняются.

138 повреждений records обнаруживает независимый classic oracle.
30 metadata refusals происходят до allocation с неизменными owner/REG.
Все 39/40 constructor allocation failures проверены на полный откат.
Три active controls искажают source field offset, destination offset и
Context descriptor relocation; каждый обязан дать смысловой отказ fixture.

Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI;
локальное исполнение относится к amd64. Classic wire format и 32-bit paths
сохраняются.

## Настоящий AWM

Неизменённый `awm.init` проходит Context allocation PC100 и инициализацию
его полей. Первый отказ теперь PC116: local call fatal. Его helper kill
останавливается на PC1245 — подготовке вычисленного String argument для
Sys.open (`/prog/` + string pid + `/ctl`). После fatal в init также остаётся
создание каналов. В Wmsrv по-прежнему закрыты managed channel creation и
серверный event loop.

Все 16 classic startup artifacts совпадают с обычной компиляцией.
Полный AWM init, Wmsrv service и Android UI ещё не запущены.

Локально gate прошёл UBSan и ASan/UBSan со всеми тремя controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает allocations/refs).
Чистая amd64 консоль, paired boot и 324 настоящих emuinit/Arg boot-процесса
прошли. Тройка каналов и прежние Context fields также прошли. В двух старых
root-map проверках сохранена уникальность адресного шаблона старого Context
active control; сами predicates не ослаблены.

Продолжение: [String preparation](native64-string-prep.ru.md) закрывает
kill/fatal и переносит первый compiler refusal AWM init на PC120 INEWCP.
