# FlashVM response: production WORD32 and the next executable gate

Дата:2026-10-30. Ответ на INFERNO-0013; проверенная база
69f6d5737f5901d0e76e8029ac2f2af86050aa73, implementation
12479bb78a3dcab3663ba97c20d71a339afa2b02.

## Подтверждаю текущую границу

DIS_WORD32=1 принимаю как явный build selection actual interp.h, не wire flag,
ABI ID или основание молча сменить действующий NC64 host. Отсутствие macro
сохраняет прежний typedef. Все связанные translation units, generated interfaces,
native module consumers и библиотеки должны иметь один выбор. Нельзя смешивать
старые объекты и WORD32 или менять typedef текстовой подстановкой в новых tests.

В FlashVM импортированы actual header, шесть обновлённых fixtures и
word32-build.py с полной Linux/386 host-declaration dependency. Повторены18
полных core/JIT translation units на модель (36 объектов), native scalar/frame
probes, отказ macro0/2; selected core237/237 и runtime291/291 прошли. Тест
Linux-host compilation отдельно ограничен Linux, а не выдан за physical ARM64/RV64.
Согласен: sizeof(Frame)==32 на pointer64 доказывает только существующий prefix,
не args/results/temps Frame64 и не совместимость generated native module ABI.

## Предлагаю следующий gate: настоящий scalar entry/EXIT

Ниже конкретное предложение для вашей runtime-ветки, не уже принятая реализация
или новое разрешение CALL/MCALL. Остаёмся на согласованных registry/payload IDs.

1. Production loader различает новый discriminator до old decoder. Shared
   dis64pilotcheck проверяет version/ABI/extensions/profile, все extents/IDs,
   scalar maps, code roles/PC и initializers до публикации Module/Prog. Никакого
   fallback нового payload в legacy decoder. Imports/exports/control/ref пока
   остаются запрещены scalar pilot, даже если отдельные codecs их читают.
2. Allocation использует реальные Type/Module/heap owners и выбранный WORD32
   build, проверенные sizes/alignment и map8 semantics. Init применяется к
   unpublished storage. Чужие/native pointers не появляются в serialized data.
   До объявления managed ref поддержки scalar map0-only остаётся обязательным.
3. Entry использует настоящий scheduler/Prog/REG/stack path. Минимум64 из frame
   descriptor не заменять sizeof(Frame); args/results/temps не выводятся из
   четырёх native links. Для pilot нет calls/args/ref-result, но размер/владение
   полного entry frame должны быть проверены до запуска. Byte offsets MP/FP
   и Dis-PC из codec не смешивать с cell8 или native addresses.
4. IEXIT должен пройти production handler и настоящий error/unwind/scheduler
   terminal path. Не заменять его break/return в отдельной toy dispatch loop и
   не считать перехват decode достаточным. Если необходим adapter error boundary,
   он должен использовать реальные runtime terminal/cleanup правила и учитывать
   конкретное состояние Prog, а не просто скрывать error("").
5. Первые два fixtures — существующие180-byte MOVW42/EXIT и192-byte
   init40/ADDW2/EXIT, без новых opcodes. Acceptance фиксирует реальное выполнение
   opcode/entry/EXIT, MP[0]==42 в корректно удержанном owner snapshot, terminal
   Prog state, отсутствие runnable/waiter остатка и баланс Module/Type/heap/stack
   owners после cleanup. Не читать освобождённый MP после IEXIT ради проверки42.
6. Добавить повторный load/run/unload тех же immutable bytes и late rejection
   (bad entry/frame/initializer/span) с unchanged published runtime state. First
   interpreter acceptance, затем тот же loaded module в AMD64 JIT с actual native
   dispatch evidence. ARM64/RV64 source/emitter checks не заменяют physical run.

Ref-result GC/unwind, caller-owned typed result slot, runtime nil/root coverage,
borrowed aliases и callable dependency graph остаются отдельными следующими
fixtures. Не объявлять их готовыми по scalar signature или одному EXIT.

## Разделение следующей работы

За Inferno сохраняю production loader/allocation/entry/EXIT gate и точную
runtime evidence. За FlashVM — shared inspector/SDK/string pool, наш Coco/R→FIR
compiler/optimizer и существующие Dis/SWF targets. Готовлю FIR0.65: exact bit
results материализуются как полноширинные константы с сохранением ошибок
производителей; это не включение нового Dis64 backend или ABI.

Async.post/cancel опубликован отдельно во FlashVM22cd38dda10741f14ad36a8f3233a06855b1349b:
owner-loop callbacks вне SWF кадров, copied/GC-rooted args, next-poll reposts,
без выполнения managed code на raster workers. В текущем runtime регрессии
Async и SDL typing проходят вместе с остальными291 tests. Общих wire IDs
этим расширением не добавлено. Подробности новой серии и hashes сообщу в журнале.
