# Native64: подготовка String внутри fixed call

Checked compiler принимает последовательность `string int`, до восьми
конкатенаций во временный FP String, конкатенацию в pending imported
argument и немедленную очистку временного root. Это позволяет целиком
проверить настоящие `awm.kill` и `awm.fatal`, включая вычисление пути
`"/prog/" + string pid + "/ctl"` для Sys.open.

Private annotations 180/181 описывают подготовку/очистку imported argument.
Annotation 182 описывает очистку временного root после передачи в local
call, например `fatal(sys->sprint(...))`. Это внутренние checked records;
classic Dis wire format и 32-bit code paths не меняются.

Compiler сверяет исходный тип каждого операнда, идентичность compiler
временного `.t`, pending frame и formal offset. Owning module проверяет
единый call island, scalar/root maps, точный временный slot, mapped formal
и readonly canonical nil. Не принимаются orphan clear, чужой call owner,
неограниченные выражения и bigint/real conversions внутри такого call.
Materializer использует обычные ICVTWC/IADDC/MOVP; счётчики ссылок, Unicode
и исключения обслуживаются существующими production String operations.

## Проверки

`utils/tests/native64-string-prep.py` строит девять настоящих Limbo bodies
и сверяет records с независимым classic decoder и canonical module headers.
120 повреждений conversion/concat/clear records обнаружены; classic bytes
совпадают с обычной компиляцией. Bigint/real source boundary остаётся закрытой.

4080 C/JIT случаев с quanta 1/3/1000 проверяют:

- signed Word limits, отрицательные значения и ноль;
- FP/MP String, ASCII, Rune, встроенный NUL, nil и пустые строки;
- свежий и унаследованный result address, дальнейший local call;
- ошибки первого/второго String allocation, native callback и отсутствующий
  imported module, сохранение старого результата;
- точное число ссылок до вызова и полное освобождение frames/Strings/modules.

Native sink контролируется тестом: настоящий Sys.open и файловый I/O
здесь не выполняются. Проверяются production cvtwc/addc/stringdup/cvtup и
реальные frame/call/exception paths. 24 metadata refusals происходят до
allocation с неизменными owner/REG; все 37/38 constructor allocation failures
проверяются на откат. Три active controls портят числовой source, external
clear и local clear; каждый должен нарушить соответствующий runtime predicate.

Gate включён в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI, а также
в native64-port.py. Локальное исполнение относится к amd64.

## Барьер AWM

Неизменённые kill и fatal принимаются целиком. `awm.init` проходит старый
барьер PC116 и останавливается на PC120 INEWCP — создании managed канала.
Это compiler diagnostic, а не запуск полного init. В Wmsrv.init остаётся
создание канала PC27 и последующая серверная логика. Все 16 classic startup
artifacts сохраняют прежние bytes. Полный AWM, его event loop и Android UI
ещё не запущены.

Локально прошли UBSan и ASan/UBSan со всеми тремя active controls
(`ASAN_OPTIONS=detect_leaks=0`, allocations/refs считаются самим fixture).
Регрессии: Context creation/copy, Lists, computed String arguments и полный
emuinit/Arg. Чистая amd64 console, paired boot и 324 emuinit/Arg boot-процесса
прошли. Сохранён отдельный materializer branch для IADDC: старый active
control снова адресует ровно публикацию concat middle operand.
