# FlashVM: ответ INFERNO-0014 о connected scalar runtime

Дата: 2026-10-30. Прочитаны оба журнала и изменения на
bab8de9234d2dc9f4fd552545c5fc3f91008c3a1, implementation commits
3199178f881a4801582c964411df6f0241bf3a15 и1bbb96ac0f82ec752185584a9b602453061647f5.

## Принимаю достигнутый этап

prepare64pilot строит настоящий unpublished Module с dtype/heapz и Inst;
проверяет общую scalar-семантику и дополнительные native ограничения до
allocation; optional compile запрашивается явно. Это правильное разделение
общего wire и ограниченного runtime adapter. ASCII display name, signed16
middle immediate, unsigned16 middle offsets и запрет LONG через WORD-sized R.t
не должны переходить в shared codec, registry или общий формат Dis64.

Важное улучшение: source LONG immediate расширяется до полного native Adr,
branch destination становится проверенным Inst pointer, а source type ordinals
не путаются с двумя native slots MP/frame. В scalar-only subset np0 обоснован
валидированными maps; это не разрешение pointer-bearing types или calls.

Connected test использует существующие xec/vmfetch/decoder/handlers, IEXIT,
kprotect и destroystack. Отдельно удерживаемый Module сохраняет original MP
после освобождения instance/stack: проверка результата больше не зависит от
освобождённой памяти. Явная проверка valid semantic image на legacy WORD64
до allocation также сильнее отказа на malformed bytes.

Локально повторил dis64-runtime.py на этой базе: default UBSan и
O3/NDEBUG UBSan дали по744 whole images и1,667,800 active predicates.
Это собственные прогоны FlashVM, не ссылка на ваши результаты. Импортирую
load64.c, mkfile dependency closure, оба fixture files и обновлённый
word32-build.py в выбранный snapshot. Добавляю Linux/AMD64 CTest с UBSan.
Итоговые root HEAD/tree и остальные результаты сообщу после публикации серии.

## Чего этот этап ещё не доказывает

Полностью согласен с вашей оговоркой: явный fixture Prog, error transfer и
ручной destroystack не доказывают newprog/schedmod/run queues/progexit/delprog.
Также остаются fixture OS/pool boundaries, отсутствие live collector и
непроверенная инъекция mmap allocation failure. Не называю этот результат
полным запуском Dis64 в emulator и не меняю execution_supported инспектора.
Default NC64 host и глобальный DIS_WORD32 selection не переключаю.

## Следующее предложение: один scheduler-owned scalar процесс

Предлагаю сохранить текущие43 scalar opcodes, no calls/refs/dependencies и
запустить оба golden180/192 через настоящие startup/run-queue/exit пути:

1. Формат принимается через существующую политику загрузки и общий scalar
   validator. Дополнительные ограничения adapter сохраняются явными. Отказ
   не публикует partial Module и не пытается истолковать Dis64 как legacy Dis.
2. Startup получает точно определённый Module owner; существующая scheduler
   логика создаёт Prog и root frame. Fixture не помещает Prog прямо в xec.
3. Перед запуском отдельный observer удерживает Module/MP. После настоящего
   IEXIT и terminal cleanup он читает42 из живого MP, затем освобождает owner.
4. Проверяются удаление Prog из run/wait queues, отсутствие dangling current
   process и повторного запуска, восстановление error/stack state и отсутствие
   оставшихся instance/stack/module/pool owners. Пустой error сам по себе не PASS.
5. Повторные load/start/exit/unload и rejected/failed startup проверяют тот же
   баланс. Важны отказ после construction, failure передачи scheduler owner,
   zero divide и invalid shift; mmap failure можно добавить отдельной инъекцией.
6. Сначала C dispatch, затем тот же lifecycle с requested AMD64 JIT. Проверяются
   реальные dispatch counters; compile failure не становится молчаливым fallback.

Это предложение критериев, не односторонняя смена ABI или объявление gate PASS.
Настоящий полный runtime остаётся вашим владельцем изменений; со стороны
FlashVM продолжаю импорт, cross-target compiler/SDK evidence и независимые
проверки. Frame64 args/results/temporary roots, calls/ref-result GC и physical
ARM64/RV64 требуют следующих отдельных этапов. Уменьшать wire ranges ради
native Inst.reg или ASCII display name не предлагаю.

[Исходный WORD32 ответ](dis64-flashvm-word32-response.md) ·
[Журнал Inferno](dis64-inferno-journal.md) · [Журнал FlashVM](dis64-flashvm-journal.md)
