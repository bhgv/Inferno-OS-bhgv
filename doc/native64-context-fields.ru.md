# Draw.Context в checked native64

В checked LOAD lowering добавлены чтение, запись и сравнение с nil трёх
полей `Draw.Context`: `display`, `screen`, `wm`. Владелец Context и второй
операнд могут находиться в FP или MP. Classic смещения 0/4/8 преобразуются
в native 0/8/16; сравнения указателей используют IBEQL/IBNEL.

Compiler проверяет nominal Draw.Context, его полный layout (24 native байта,
три mapped roots), выбранное поле и managed класс второго операнда. Обычный
ADT с тем же layout и Context другого namespace не допускаются. Nested field
access, общий ADT ABI и implicit admission произвольных файлов здесь не добавлены.

Доверенный Native64TypedStep использует private annotations: 96 — IMOVP поля,
его смещение передаётся в m; 97/98/99 — nil branch для display/screen/wm,
m остаётся смещением nil root в MP. Это не сериализованные Dis opcodes и не
контракты возврата функции. Native materializer проверяет режимы и spans,
module constructor — root maps обоих операндов, границы branch и отсутствие
операции внутри pending call setup. Classic Dis, обычные frame layouts и
32-битный ABI сохраняются.

## Воспроизведение

```sh
python3 utils/tests/native64-context-fields.py --limbo /path/to/host-tools/limbo
```

Gate компилирует семь небольших Limbo функций и исполняет их через реальные
scheduler/stack, owning modules, linkmod MP instance, C interpreter и JIT.
252 вызова покрывают quanta 1/3/1000, FP/MP Context, все три поля, замену
существующих ссылок, очистку nil, nil owner, все пустые поля и каждое отдельное
пустое поле. Проверяются точные значения, FP locals, запись той же ссылки обратно, reference counts, caller guard и
полное освобождение owners, heaps и Types. Дочерние Display/Screen/channel
payloads в fixture непрозрачны: Draw service и обмен по каналам не исполняются.

180 повреждений field/branch records должен обнаружить независимый classic
oracle. Ещё 22 malformed native records отвергаются до allocations; все
23/24 allocation failures конструктора C/JIT откатываются. Контрольные
удаления field displacement, branch displacement и root-map guard должны
сломать gate. Sanitizer-прогон использует ASan/UBSan; LSan отключается в
окружении с ptrace, allocations дополнительно учитывает fixture.

Новый gate включён в native64 suite и CI Linux amd64/arm64/riscv64 и Android
ARM64 Bionic. Локальный C/JIT прогон относится к amd64; включение CI не является
утверждением о запуске Android UI.

## Что изменилось для AWM

В полном неизменённом `appl/wm/awm.b` checked compiler проходит прежний отказ
PC51 (`display = ctxt.display`) и следующий nil test `ctxt.wm`. Первый отказ
теперь PC61: IMFRAME вызова `wmclient->win.reshape(((0,0),(100,100)))`, где
нужна передача Rect в imported frame. Это compiler evidence, а не выполнение
AWM init. Classic bytes всех 16 startup-модулей совпадают.

Следующий рубеж — aggregate arguments/results Point/Rect, затем оставшиеся
Draw/WM services и каналы/alt. Полное окно AWM и Android UI ещё не запущены.

Локально прошли UBSan и ASan/UBSan с тремя active controls, полные emuinit/Arg
owning gates, прежние 180 coordinate C/JIT случаев, managed file client с
реальными GC epochs и 36 literal proof images. Чистая amd64-консоль собирается
и проходит paired boot и 324 реальных emuinit/Arg boot-процесса.
