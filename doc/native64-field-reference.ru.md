# Native64: сравнение ref из поля с прямым ref

Настоящий awm.init проходит `win.image != screen.image` на PC178. Следующий
отказ — PC180 `multi-callee` на вызове reshaped: сам reshaped останавливается
на PC716, IMOVM при чтении Image.r в локальный Rect. Command остаётся accepted.
Полный event loop, графическое окно и Android UI ещё не запущены.

## Контракт

Ранее выбранный путь обслуживал сравнения поля с readonly nil. Новый role 207
обслуживает равенство/неравенство двух Tref: одно значение из проверенного поля,
другое из прямого FP/MP root. Оба операнда сравниваются полными 64 битами через
IBEQL/IBNEL; число int остаётся 32-битным. Перестановка операндов сохраняет
семантику equality/inequality. Прежний путь field/nil сохраняется.

| Владелец | Выбранные ref fields | Family |
| --- | --- | --- |
| Draw.Image | display, screen | 1 |
| Draw.Screen | image, fill, display | 1 |
| Draw.Display | image, white, black, opaque, transparent | 1 |
| Wmclient.Window | display, image, ctxt, titlebar, screen | 2 |
| Draw.Context | display, screen | 3 |
| Wmsrv.Client | znext, wmctxt | 4 |

Всего 19 полей. String, channel fields, чужие ADT и два косвенных операнда
одной инструкции этим контрактом не открываются. Nominal owner/layout доказывает
компилятор через существующие field proofs; classic Dis bytes не меняются.

Checked middle хранит family в bits 24..26, native field offset в 16..23,
прямой FP/MP offset второго ref в 0..15. Материализация разделяет эти значения
между source indirect operand и обычным native middle register. Проверяются
family/field whitelist, opcode/modes, alignment/extent обоих roots, GC bits,
границы branch target и call-site ownership. Packed field branch без правильного
role также отвергается до allocation, чтобы повреждение tag не обходило proof.

## Проверка

`utils/tests/native64-field-reference.py --limbo PATH` строит 76 полных Limbo
bodies: 19 полей и четыре сочетания FP/MP owner/peer. Используются канонические
.m signatures, generated C layouts и GC maps всех owner/value ADT. Независимый
classic operand/branch oracle обнаруживает 912 повреждений records.

2736 C/JIT исполнений проверяют equality/inequality, nil field/peer, alias,
два различных объекта с одинаковым содержимым, nil owner, quanta 1/3/1000,
точную ветку, соседний Word guard, сохранность всех owner bytes и тождество
сохранённых roots. Проверяются точные refs объектов/Types и полное освобождение
frames/modules/environments. В число запусков входят 456 nil-owner unwind.

Четыре дополнительных C/JIT проверки исполняют тот же материализатор и branch
на raw register-bit sentinels с одинаковыми младшими 32 битами и разными старшими.
Эти биты не выдаются за heap owners и не включаются в GC maps. Контроль,
подменяющий IBEQL/IBNEL на 32-битное сравнение, обязан обнаруживаться ими.

1976 повреждений метаданных отвергаются до allocation; все 161/162 точки отказа
конструктора C/JIT откатываются. Ещё два active controls подменяют field offset
и снимают проверку второго GC root. Все controls обязаны давать semantic
predicate failure без sanitizer crash.

Gate включён в native64-port.py, Linux amd64/arm64/riscv64 и Android ARM64
Bionic workflow. Локальное исполнение относится к amd64; другие архитектуры
здесь не объявляются проверенными. Код 32-битной сборки сохранён.

Локально прошли UBSan и ASan/UBSan со всеми тремя active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает owners/allocations).
Регрессии: Context 252, Window 378, Draw fields/calls 1162 и Client 4032
C/JIT сценариев со всеми controls. Три старых field-displacement matcher
уточнены до общей Context/Window/Draw ветки, чтобы отдельно добавленная Client
ветка не давала неоднозначного совпадения. Чистая amd64 console/paired boot
сборка и 324 настоящих emuinit C/JIT процесса прошли; окно не запускалось.
