# Native64: mapped channels с tuple payload

Checked compiler допускает INEWCMP для плоского tuple из двух или трёх
managed roots: String, Ref, Chan, Array, List и Module. Это покрывает все три
payload из настоящего Wmsrv.init, включая Sys.Rwrite как channel root.
Mixed scalar/root, raw, four-root, nested tuple и by-value ADT не открыты.

Проверяются исходный channel/tuple payload, canonical descriptor ordinal,
legacy size8/12 и map C0/E0, а также полный native layout16/24 с alignment8.
Compiler может объединять descriptors с одинаковой GC geometry. Поэтому
layout проверяется отдельно у фактического payload и canonical descriptor:
оба обязаны иметь одинаковое число root cells и те же offsets/размеры/map.
Каждое поле должно быть непосредственным root, а не вложенным aggregate.
Ёмкость и output используют общие с primitive channels проверки FP/MP,
signed Word, immediate и borrowed result address.

Private checked roles183/185 обозначают local/global и borrowed result.
Их source — root count2/3, не файл с native address и не старый ordinal.
Owning constructor создаёт отдельные Type16/C0 и Type24/E0 и переписывает
source operand INEWCMP в native ordinal. Эти Types добавляются после byte
и tuple-copy descriptor, перед Context: его прежний последний ordinal
сохраняется. Все пять identities проверяются отдельно даже при одинаковой
geometry24/E0. Classic Dis bytes и 32-bit paths не меняются.

C использует production newcmp/newc, JIT — cnewcmapped либо обычный fallback.
Mapped mover удерживает payload Type и для unbuffered channel; buffer
удерживает его отдельно. freechan/freearray освобождают оба retains и все
payload roots.

## Проверки

`native64-mapped-channels.py` строит 17 полных Limbo functions с настоящими
Wmsrv/Draw/Sys declarations и source boundary для закрытых форм. Независимый
classic oracle обнаруживает 78 повреждений opcode/mode/operands/role.
Старые classic bytes совпадают с обычной компиляцией.

1938 C/JIT случаев с quanta1/3/1000 проверяют:

- реальные двух- и трёхкорневые Wmsrv payload layouts;
- FP/MP/immediate/zero capacities и обе signed int границы;
- local, borrowed, global и replacement outputs;
- отрицательные/слишком большие размеры и ошибки каждого из четырёх resources;
- точную Type/mover identity, nil initialization всех buffer root cells;
- несколько алиасов managed payload, точные refs и освобождение каждого root;
- полное освобождение frames/modules/queues/channels/buffers и descriptors.

Payload objects в fixture — opaque managed handles; send/receive и Wmsrv
server protocol не выполняются. 24 malformed metadata случая отказывают до
allocation с неизменными owner/REG. Все 49/50 constructor allocation failures
проверены на откат. Active controls ломают descriptor relocation и удаляют
scalar/root-map guard ёмкости; semantic predicates обязаны это обнаружить.

Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI, а также
native64-port.py. Локальное исполнение относится к amd64.

## Барьер запуска

Wmsrv.init проходит mapped channel creation PC27–29. Первый отказ теперь
PC30: IFRAME перед ISPAWN, запускающим wm event loop. AWM init остаётся на
spawn setup PC127/ISPAWN PC131. Следующий этап требует scheduler/callee proof,
а не ещё одной геометрии root. Полный AWM, Wmsrv service и Android UI пока
не запущены.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; refs/allocations отдельно проверяет fixture).
Регрессии: primitive channels, Context creation/copy, channel triples и полный
emuinit/Arg со своими controls. Чистая amd64 console, paired boot и 324
настоящих C/JIT emuinit/Arg boot-процесса прошли. Все 16 classic startup
artifacts сохраняют обычные bytes. Bounded geometry proof не добавляет
зависимости от полного type checker в автономные checked-IR fixtures.

Продолжение: [checked local spawn](native64-spawn.ru.md). Scheduler path
проверен на реальных детях/внуках; actual startup теперь требует admission
полных spawned command/wm handlers.
