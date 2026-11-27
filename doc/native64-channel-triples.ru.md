# Native64: результат из трёх каналов

Checked LOAD compiler допускает результат `(chan, chan, chan)` с любыми
проверенными Limbo payload types. Classic layout12 переносится в native24:
три указателя по offsets0/8/16. Scalar/mixed tuples и четыре канала остаются
закрытыми. Это private IR contract72, без изменения classic wire format.

## Контракт и lifetime

Fresh result занимает три последовательных mapped FP cells; inherited call
передаёт уже проверенный адрес такой же тройки. MP tuple binding не добавлен.
Для local target и imported declaration constructor проверяет отдельное
доказательство resultcontract72. К imported metadata добавлен resultcontract;
прежние declarations имеют нулевое значение. Hash и generated frame prefix
по-прежнему проверяют идентичность импортируемой функции.

Каждый MOVP return component имеет отдельную relocation/annotation. Возврат
локальной тройки одной IMOVMP использует отдельно созданный native Type24,
map E0. Он сохраняет все три корня перед освобождением заменяемых значений.
Если в модуле нужны и byte-array Type, и tuple Type, их ordinals различаются.
Byte-array descriptor сохраняет прежний index.

Definite-result analysis отслеживает три независимых component facts на всех
достижимых путях. Очередь рассчитана на четырёхбитную lattice; inherited и
mapped-copy return определяют сразу всю тройку. Runtime binder проверяет
все 24 байта, три root bits, alignment, extent и pending frame до публикации
адреса результата. Старые scalar, pair48, MP root24 и fstat88 контракты
сохраняются.

## Исполнение

`utils/tests/native64-channel-triples.py` проверяет 12 Limbo functions:
прямой/nil возврат, local forwarding, fresh destructuring и mapped return,
ветвление, перестановку/алиасы, imported fresh/inherited calls, сохранение
трёх channels в globals. Отдельный wrapper использует настоящую декларацию
`Wmsrv.init` из `wmsrv.m`, canonical hash и C prefix, полученные `limbo -H`.

400 C/JIT случаев включают quanta1/3/1000, stack growth, nil/alias cases,
ошибку native producer, отсутствующий module и 40 runtime map/span/NP faults.
Используются настоящие unbuffered Channel objects, production cnewc/freechan
и production MOVMP. Fixture предоставляет heap allocator и teardown пустых
очередей; обмен сообщениями не выполняется. Controlled Wmsrv callback проверяет
только передачу трёх channel handles, не запускает Wmsrv service и его payload
protocol.

Проверяются точные refs и полное освобождение channels, queues, descriptors,
MP clones, frames и modules. Все 43/44 constructor allocation failures дают
полный откат. 40 повреждений metadata отвергаются до allocation, materializer
отдельно отвергает отсутствие callee-result proof с неизменным output.
Независимый classic oracle обнаруживает 120 component/binding corruptions;
обычные classic bytes совпадают. Десять compiler CFG cases включают отсутствие
каждого компонента, diamond/loop joins и inherited/mapped result facts.

Шесть active controls убирают проверку определения третьего компонента,
искажают return offset, удаляют третий root из copy Type/runtime binding и
удаляют callee-result proof и смешивают byte/copy ordinals. Каждый обязан вызвать смысловой отказ теста.
Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI.
Локальное исполнение относится к amd64.

## Настоящий startup frontier

Неизменённый `awm.init` проходит вызов `Wmsrv.init` на PC97; новый первый
отказ PC100 — INEW для `Draw.Context`. Сам неизменённый `Wmsrv.init` проходит
описание tuple result и первые вызовы, но останавливается на PC27 — INEWCMP,
создании managed channel. Затем остаются spawn и серверный event loop.

Все 16 startup sources сохраняют classic bytes. Это compiler frontier и
bounded runtime proof передачи результата: полный Wmsrv service, AWM window,
Android APK/UI ещё не запущены. 32-bit пути сохраняются.

Локальный gate прошёл ASan/UBSan вместе со всеми шестью controls
(`ASAN_OPTIONS=detect_leaks=0`; allocations/refs отдельно считает fixture).
Прежние makescreen/Draw, emuinit/Arg, fstat, Command, module/external/multi
regressions прошли. Чистая amd64 консоль, paired boot и 324 настоящих
emuinit/Arg boot-процесса также прошли. Android UI и физические ARM64/RISC-V64
устройства этим запуском не проверены.

Продолжение: [создание Context](native64-context-new.ru.md) переносит первый отказ AWM init с PC100 на PC116.
