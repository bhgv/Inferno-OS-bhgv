# Native64: nil-проверки каналов и каналов Client

Настоящий awm.init проходит rc == nil на PC257, c.stop == nil, удаление
клиента, обновление focus и следующий setfocus. Новый отказ — PC303
multi-callee на handlerequest. Сам handlerequest пока останавливается на
PC427 client-context-geometry: выделение ref Draw.Pointer.
setfocus, controlevent и reshaped остаются accepted целиком. Это проверка
компилятором checked-профиля; графический AWM и Android UI пока не запущены.

Добавлены две формы checked LOAD:

| Role | Источник | Сравнение |
| --- | --- | --- |
| 239 | Прямой channel root в FP/MP | IBEQL/IBNEL с readonly nil MP |
| 241 | Канал внутри канонического Wmsrv.Client FP/MP | Поле0/8/16/24/64 с readonly nil MP |

Для прямого канала payload не влияет на проверку: сравнивается сам handle,
содержимое канала и его очереди не разыменовываются. Можно проверить nil
для любого типизированного канала. Сравнение двух channel handles этим
этапом не добавляется.

Для Client выбираются только kbd, ptr, ctl, stop и images. Проверяется
полный nominal layout Client, classic52/native96 и все 13 leaf. Другие
ссылочные поля Client, строки и произвольные ADT этим путём не открываются.
Nil Client вызывает штатный unwind до чтения поля; nil канала является
обычным сравниваемым значением. Указатели сравниваются по всем 64 битам,
а не по младшему Word32.

До выделения проверяются opcode/modes, source root8, nil root8, его
readonly значение и map, локальная цель ветвления и отсутствие операции
внутри подготовки CALL. Для поля отдельно проверяются точный offset и
упаковка offset/nil location в metadata. Nil MP operand ограничен 65535,
поскольку Inst.reg имеет 16 бит. GC map сам по себе не доказывает тип
heap-объекта; premise о channel/Client поступает из compiler IR. Загрузка
произвольного native файла не разрешается.

Новый native64-channel-nil.py проверяет:

- 44 функции, 1320 C/JIT случаев: FP/MP, eq/ne, четыре payload формы
  (int, String, tuple(int,String), ref Pointer), все пять полей Client,
  три quantum, живой/nil channel и nil Client.
- Используются настоящие cnewc и структуры Channel, включая map-aware
  payload. Проверяются очереди/владение через штатное освобождение всех
  объектов, caller guards и отсутствие изменения результата при nil-owner.
- Для MP probe и полей проверяются четыре непрозрачных значения указателя,
  в том числе с младшими 32 битами, совпадающими с nil. Их не разыменовывают
  и не retain; GC в probe не запускается. Значения восстанавливаются до
  освобождения module/Client.
- Независимый classic Dis oracle обнаруживает 264 повреждения записей;
  исходные bytes и canonical signatures сверяются отдельно.
- 704 повреждения metadata отвергаются до выделений; constructor rollback
  проверен в 97/98 точках C/JIT.
- Активные контроли обнаруживают усечение сравнения до32 и подмену
  stop24 на kbd0. Требуется смысловой отказ, а не sanitizer crash.

Gate подключён к native64-port.py и amd64/arm64/riscv64 CI. VM handlers и
backend JIT не менялись. Классический Dis и 32-битные пути сохранены.
Локальное выполнение этого этапа — amd64; остальные архитектуры и Android
UI в этой сессии не запускались.

Локально прошли UBSan и ASan+UBSan (`detect_leaks=0`, со счётчиками полного
освобождения), включая оба активных контроля. Регрессии reference fields2736
и Client fields/channels4032 прошли со своими контрольными мутациями.
Отдельная чистая amd64 сборка прошла console/pair boot и 324 процесса
настоящего Arg/emuinit.
