# Следующий барьер emuinit: ref Sys.FD и nil

Checked LOAD lowering теперь допускает сравнение прямого локального/аргументного
`ref ADT` с readonly `nil` из MP. Это сравнение полного 64-битного корня через
IBEQL/IBNEL. Проверяются базовый адрес декларации, отсутствие смещения поля,
режимы операндов, тип ref/ADT, карта roots и принадлежность перехода функции.
Содержимое ADT не читается; геометрия и поля FD этим не допускаются. Прежние
правила Module и list of string сохранены; ref/ref по этому пути закрыт.

В настоящем неизменённом `appl/cmd/emuinit.b` getenv проходит прежний отказ
PC101. Новый первый отказ — PC104, MFRAME Sys.fstat с aggregate результатом
`(int, Sys.Dir)`. Это проверяется новым gate по classic import table, а не по
одному номеру PC. Байты classic Dis не меняются. Полный getenv/emuinit/AWM
по-прежнему не исполняется.

`native64-emuinit-fd.py` компилирует четыре полные функции против настоящего
module/sys.m: == nil, != nil, обратный порядок и локальная копия. Независимый
oracle сверяет classic операнды, maps, native offsets и замену word-branch
на long-branch. Реальные FRAME/CALL/RET, String-independent heap root, C/JIT,
рост стека и три кванта дают 96 исполнений с проверкой результата, canaries,
refcount и полного teardown. Heap root здесь служит непрозрачным ref:
это не вызов Sys.open/fstat и не проверка C payload FD.

IR unit проверяет 11 неверных field/role/map/branch вариантов; активное
удаление запрета field offset обнаруживается. ASan/UBSan с NDEBUG проходит.
Gate добавлен для amd64, arm64, riscv64 и ARM64 Bionic в CI. Статическая
host-сборка с Android headers проходит отдельно от целевого Bionic. В ней
используется штатный Bionic free_ seam: внутренние libc free не должны
попадать в VM allocation ledger.

Также исправлены две неоднозначные контрольные мутации прежних тестов:
верхний предел immediate middle operand и table index imported call теперь
выбираются по точному контексту. Обе мутации по-прежнему обязаны приводить к
своему диагностическому отказу. Лимит CI увеличен до 90 минут: предыдущий
amd64 run доходил только до typed-calls уже на 78-й минуте scalar workflow;
typed module workflow остановился на том же тесте примерно через 35 минут.

Отдельно прогнаны существующие sys-stat-slots: по 816 случаев для emu/os и
WORD32/WORD64, включая tuple/string/native-buffer/failure cleanup. Наличие
работающих Sys_fstat handlers не означает готовность checked aggregate-call
контракта: пока он отвергается до публикации native тела getenv.

Прежний startup-boundary fixture теперь ожидает принятия своего прямого
`ref R == nil`; три прежних запрета (nested list, array, module/module)
сохраняются. Новая возможность таким образом явно отражена и в старом gate.

Для следующего изменения проверен текущий `limbo -H -M Sys module/sys.m`:
Sys.Dir занимает 80 байт, length находится по смещению 64; F_Sys_fstat —
72 байта, fd по смещению 64, map {0x00,0x80}. Результат имеет status по
смещению 0 и Dir по смещению 8 (всего 88 байт, четыре String-корня).
Следующий контракт обязан проверять этот полный диапазон и корни, включая
замену уже принадлежащих вызывающему Strings и cleanup при исключении.

Продолжение: [реализованный checked Sys.fstat result](native64-emuinit-fstat.ru.md).
Текущий первый барьер getenv теперь INEWA (PC112), а не fstat.
