# Строковый raise в checked LOAD

IRAISE допускается только в выбранном LOAD lowering, с прямым mapped String
операндом FP/MP, без middle/destination/result. Прежние scalar/external-only
профили и функции с собственными exception handlers остаются закрыты.
CFG считает raise терминальным: исключение не требует заполнения результата,
но каждый обычный возврат пары требует обоих полей. Constructor проверяет
геометрию и root bit источника до аллокаций. Wire IDs и 32-битный ABI не меняются.

Gate raises использует настоящие iraise и unhandled handler из emu/port.
Он проверяет fail:error после nil LOAD, замену прежнего Module на nil,
процессный exval root, retain/release сообщения и неизменность REG при cleanup.
Прямое MP сообщение и локальное FP сообщение проверяются отдельно.
Сохраняются проверки argv, парного результата, namespace и прежние controls.
Новые controls отключают mapped-source guard, retain исключения и освобождение
unhandled owner; CFG unit отдельно отключает terminal edge.

Запуск: python3 utils/tests/native64-typed-raises.py --limbo /path/to/limbo
Gate включён девятнадцатым для x64/ARM64/RV64/Android Bionic.
Это ограниченная проверка startup exception path, а не полный запуск AWM.

Отдельная правка cb102a4 исправляет compiler unit с Android allocator aliases.

Основной путь текущего appl/cmd/emuinit.b: load Command
"/dis/wm/awm.dis", затем mod->init(nil, args). Shell загружается только
при отказе прямой загрузки. Этот gate проверяет аварийный участок, а не
заменяет основной путь на запуск через sh.

Локально полностью пройдены x64 ASan+UBSan/NDEBUG (detect_leaks=0):
3888 обычных C/JIT запусков, 432 pair faults и 864 дополнительных nil/replacement
случая. Все три новых runtime omission controls и compiler CFG control
проверены. Предыдущий pairs gate и 960 module cases с controls, свежая
console/paired boot также проходят. Closure с Android allocator aliases
дополнительно проверяется на host; это не квитанция выполнения ARM64/Bionic.
Handler closure ограничена unhandled путём: ветви формирования диагностик
выбранного handler в fixture явно запрещены. Catch/resume этим gate не открыт.
