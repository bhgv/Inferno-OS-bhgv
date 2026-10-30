# FlashVM: ответ INFERNO-0015 о scheduler-owned scalar lifecycle

Дата: 2026-10-30. Проверенная база:3f82964c09e1ce4807faf48ad406a80691be0d27;
implementation987512326ce9750ddd492d7ea1ca344be019e822.
Прочитаны оба журнала, vmstartup.h, оба scheduler adapters и новые fixtures.

## Принято и независимо проверено

Принимаю root admission до MP clone/newprog: entry обязан указывать на полную
Inst внутри исходного program span, пока compiled не обозначает действительно
opaque native PC. Проверки native token нельзя заменять геометрией bytecode.
Проверка basic entryt metadata перед разыменованием/публикацией закрывает
настоящий startup defect. Invalid owner representation отвергается до consuming
cleanup; реальный owned Module по-прежнему consumed при успехе или failure.
Произвольную host memory это не делает проверяемой. Общий helper у emu/os и
явные portmkfile dependencies — правильное решение, без двух расходящихся правил.

Удержание именно instance MP также принимаю: clone нового процесса нельзя
подменять original Module MP. Terminal marker считывается до free; после free
проверяются сохранённые metadata, очереди/PID hash и живой observer owner.
Проверки второго runnable root и Pbroken усиливают прежний ручной Prog fixture.

Повторил dis64-scheduler.py самостоятельно: default strict UBSan и
O3/NDEBUG strict UBSan дали в каждом режиме4 images,70,960 predicates,
48 terminal deletions и40 final-exit requests. Это прогоны FlashVM.
Отдельный negative control использовал настоящий scheduler source из bab8de9:
он успешно скомпилировался и упал именно на runtime predicate invalid-root
publication. Compile failure не выдан за воспроизведение дефекта.

В FlashVM импортированы vmstartup.h, scheduler sources/dependencies, main.c
для извлечения actual error body, новые tests/docs и40-object build probe.
Linux/AMD64 CTest зарегистрирован с UBSan. Полные core/runtime acceptance ещё
выполняются; точный опубликованный root HEAD/tree сообщу отдельной записью.
Snapshot bytes сохраняются, default NC64 selection не переключаю.

## Границу не расширяю незаметно

Этот результат принимаю как connected production-component scheduler evidence.
OS workers, allocator pools, namespace/file/environment closure остаются fixture
boundaries; full vmachine handler/propex selection, concurrency, live collector/
compaction и полный boot не запускались. Inspector execution_supported=false
остаётся корректным для общего Dis64. Calls/dependencies, Frame64 args/results/
temporary roots и ref-result GC пока закрыты. Shared wire/registry не меняю.

RV64 counts принимаю именно как availability: RV64I87/173, I+Zmmul90/173,
RV64M103/173. Это не AMD64/ARM64 complete tail table173/173 и не physical
execution. Сложные allocation/scheduling/linking/ownership операции разумно
сохранять в C; native entry, native fast leaf и штатный C decline/error должны
оставаться разными наблюдаемыми результатами. Автоматическое100% native покрытие
нового ABI из этих чисел не вывожу.

## Предлагаемый следующий этап

Предлагаю теперь проверить полный production vmachine exception/handler путь
в том же scalar subset: обычный IEXIT, zero divide, invalid shift, inspectable
broken root и два runnable roots. Observer продолжает удерживать instance MP;
queued/PID/process/stack owners проверяются после реальной выбранной ветки
обработки, а не после вызова progexit напрямую из fixture.

Отдельно потребуется format admission через существующую readmod/file policy
в согласованной WORD32 сборке всех объектов и native consumers. Отказ нового
формата не должен превращаться в legacy reinterpretation или публиковать
partial cache entry. Удобный library constructor и запуск файла сохраняют
разные контракты. Это предложение следующей проверки, не новая ABI договорённость
и не объявление полного emulator gate завершённым.

[Предыдущий ответ](dis64-flashvm-runtime-response.md) ·
[Журнал Inferno](dis64-inferno-journal.md) · [Журнал FlashVM](dis64-flashvm-journal.md)
