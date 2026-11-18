# Native64: makescreen проходит checked compiler и C/JIT fixture

Неизменённый `appl/wm/awm.b` теперь допускает весь helper `makescreen`:
23 инструкции, три imported Draw calls. `awm.init` проходит local call на
PC93 и следующий первый отказ — PC97, `Wmsrv.init` с результатом из трёх
channels. Это checked IR evidence; полный AWM init и окно ещё не запущены.

## Глобальный результат

Imported и local Limbo calls могут публиковать один root result в mutable
MP slot. Compiler проверяет исходный Dglobal, точную native relocation и
root kind. Scalar globals, tuples и поля через indirect result этим путём
не добавлены. Local/imported Word globals остаются отрицательными тестами.

Constructor проверяет mapped destination и уникальный call с contract24,
без inherited result. Runtime binder отдельно проверяет module data Type,
`mptype`, NP/map, extent/alignment и pending frame. Адрес MP живёт через
сохраняемый caller module; stack growth не превращает его в адрес FP.
FP/inherited contracts и mixed fstat result сохраняют прежние проверки.
Readonly nil proof учитывает также запись через MP result binding.

## Image geometry

Разрешены только scalar Rect `Image.r/clipr` и Point-половины
`r.min/max`, `clipr.min/max`. Проверяется полная nominal Image layout и
исходный тип Rect/Point. Classic/native scalar offsets одинаковы:
Rect0/16, Point0/8/16/24. IMOVM копирует 16/8 байт в немаппированные slots
точного generated prefix получателя; owner остаётся mapped root.

Для `screen.fill.r.min` разрешена ограниченная последовательность:
Screen.fill -> FP temporary Image -> Point argument -> nil clear temporary.
Проверяются оригинальные owner/field types, связь временного slot с copy,
немедленная очистка, единственный imported call и maps источника/назначения.
Произвольные ADT и цепочки indirect fields этим не допускаются.

## Исполнение и отказы

Расширенный `native64-draw-fields.py` использует 35 Limbo functions.
Тело makescreen и Background извлекаются прямо из `appl/wm/awm.b`, без
переписывания helper. Draw headers, hashes и prefixes генерирует `limbo -H`.
Display.color/Screen.allocate/Image.draw callbacks контролируются fixture;
это не настоящий Draw graphics connection.

1162 C/JIT случая с quanta1/3/1000 включают:

- прежние 840 field cases и 54 native call cases;
- 108 global native/local result cases;
- 108 Image Rect/Point cases, nil owner и native error;
- 36 запусков исходного makescreen, ошибки color/allocate/draw, nil Image/
  Display, результат caller и оставшийся global Screen;
- 16 runtime refusals при удалении MP root bit/NP, укорочении data Type и
  неверном mptype. Старое значение result остаётся неизменным.

Считаются точные refs и полное освобождение всех objects/modules/stacks.
Перед Image.draw временная Image уже удалена: fill держат только Screen
и pending argument. Используется настоящий production MOVM handler.

816 повреждений source records обнаруживает независимый classic oracle;
32 прежних и 56 новых metadata refusals происходят до allocation с
неизменными REG/output. Все 88/89 constructor allocation failures дают
полный откат. Семь active controls искажают field/argument/Rect displacement,
global result address/root map и обе формы temporary clearing.

Проверки входят в существующий Draw gate Linux amd64/arm64/riscv64 и
Android ARM64 Bionic CI. Локальное исполнение относится к amd64.
Classic bytes всех 16 startup-модулей совпадают с ordinary compilation.
Private annotations не меняют classic wire format и 32-bit пути.

Локально весь Draw gate прошёл UBSan и ASan/UBSan со всеми семью controls
(`ASAN_OPTIONS=detect_leaks=0`; allocations/refs отдельно считает fixture).
Свежая amd64 консоль, paired boot и 324 настоящих emuinit/Arg boot-процесса
также прошли. Полный Android APK/UI и физические ARM64/RISC-V64 устройства
этим результатом не проверены.
