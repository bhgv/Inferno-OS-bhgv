# Native64: Client.flags и Screen.image в локальном CALL

Настоящий setfocus теперь accepted целиком. awm.init проходит вызов
setfocus на PC210, ptrfocus.flags, передачу screen.image.r в r2s на
PC229–234 и установку Controller в c.flags на PC240.
Новый отказ — PC243 multi-call: подготовка строки `newclient ` + string c.id
для локального controlevent. reshaped остаётся accepted целиком.
Это проверка компилятором checked-профиля; графический AWM и Android UI
пока не запущены.

Добавлены три выбранные формы:

| Role | Форма | Native payload |
| --- | --- | --- |
| 234 | n = Client.flags & immediate | flags32, чтение4, результат Word32 FP/MP |
| 235 | Screen.image → temp → Image Rect/Point → local CALL | image8, root8, копия8/16, обязательный clear |
| 236 | Client.flags |= immediate | flags32, изменение ровно4 байт |

Операции flags требуют канонический layout Wmsrv.Client: classic52/native96
и все 13 leaf с их типами и смещениями. Разрешено только поле flags,
classic16/native32, и непосредственная знаковая маска -32768…32767.
AND хранит в metadata offset32 и 16 бит маски; материализатор восстанавливает
её знак. OR использует штатный implicit middle, то есть читает старое flags
из назначения. Остальные поля, маски из переменных, read-only OR поля,
общие косвенные арифметические операции и другие ADT не добавляются.

Слот ссылки на Client обязан быть root8. Для AND весь результат4 должен
быть сырым FP/MP участком; OR пишет только известный сырой leaf внутри
объекта. Opcode, mode, маска, offset, spans и root maps проверяются до
выделений. Обе формы запрещены внутри подготовки CALL. Nil Client вызывает
штатный unwind. Классические Word32 не расширяются вместе с указателями.

Для локальной геометрии разрешена только следующая последовательность:

1. Подготовленный IFRAME локального вызова.
2. IMOVP поля Screen.image (classic4/native8) в compiler temporary root8.
3. IMOVM Image.r/clipr либо Point в сырой аргумент локального кадра.
4. IMOVP readonly nil в тот же temporary root перед CALL.

Проверяются полный Screen/Image layout существующим Draw proof, точное
тождество временной переменной и её per-use типа, карты обоих roots,
непрерывные Word32 leaf аргумента, единственный локальный callsite и
обязательная связка prep/copy/clear. Для clear отдельно проверяются
readonly nil, root назначения и raw map дочернего кадра.
Новый путь не разрешает SPAWN, Screen.fill в локальном prefix, другие
owner-классы или произвольные aggregate. Прежний импортируемый Screen.fill
с ролью166 сохраняет собственное смещение16 и внешний callsite.

Проверки на amd64:

- Новый native64-client-flags.py: 56 функций, 3024 C/JIT запуска, четыре
  сочетания FP/MP owner/result для AND, FP/MP OR, семь масок, восемь
  числовых профилей и nil, три quantum. Проверяются out, соседний живой
  локальный Word32, весь payload Client (включая padding и соседние roots),
  caller guards и освобождение владельцев. 336 повреждений независимого
  oracle; 840 отказов metadata до выделений; 121/122 точки rollback.
- Расширен native64-local-geometry-args.py: 384 C/JIT запуска вместо240.
  Добавлены MP/FP Screen.image, Rect, r.min и clipr.max, nil Screen и
  nil Image. Выполняются настоящие Limbo r2s из AWM и p2s; проверяются
  точные десятичные строки, payload Screen/Image, старый String result
  и полное освобождение roots. Oracle обнаруживает 114 повреждений;
  284 отказа metadata до выделений, включая 56 нарушений prep/copy/clear;
  40/41 точка rollback конструктора.

Активные контроли обнаруживают неверный flags offset, потерю знака маски,
снятие owner map, запись8 вместо4 в AND/OR, подмену Screen.image на fill,
короткую Rect-копию и снятые raw maps. Требуется смысловой отказ;
sanitizer crash не засчитывается. Новые операции не меняют VM handlers
и backend JIT. Обычные Dis bytes и 32-битные пути сохранены.

Оба gate подключены к native64-port.py и amd64/arm64/riscv64 CI.
Локальный прогон этого этапа не означает выполнения остальных архитектур
или Android UI.

Локально оба gate прошли UBSan и ASan+UBSan (`detect_leaks=0`, с явными
счётчиками владения), включая активные контроли. Регрессии Draw fields/calls
1162 и Client.cursor arguments192 прошли со своими контрольными мутациями.
Отдельная чистая amd64 сборка прошла console/pair boot и 324 процесса
настоящего Arg/emuinit.
