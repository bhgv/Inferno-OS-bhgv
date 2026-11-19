# Native64: Window.tag и временные String в форматтере

Компилятор принимает настоящий `awm.reshaped` целиком, включая
`sys->sprint("!reshape %q -1 %s", w.tag, r2s(nr))`, SEND в z.ctl и вторую
команду rect. В `awm.init` следующий отказ сместился с PC180 `multi-callee`
на PC187 `unsupported-opcode`: ICVTCW, преобразование String в int.
Это проверка компиляции полного reshaped, а не запуск графического AWM.
Графический AWM и Android UI пока не запущены.

## Новые операции

| Role | Операция | Что доказывается |
| --- | --- | --- |
| 222 | Window.tag прямо в String tail | nominal Wmsrv.Window, tag offset0, FP/MP owner root, полный восьмибайтовый String slot callee |
| 223 | очистка временной String | предыдущее прямое FP String argument store, та же временная, источник nil и отсутствие дальнейших читателей |

Допуск222 относится к typed String tail, не к format или FD prefix. Полная
раскладка Wmsrv.Window проверяется существующим helper. Значение String
копируется настоящим IMOVP в подготовленный кадр с удержанием ссылки; адрес
поля не передаётся форматтеру. Поэтому FP и MP владельцы допустимы: после
копирования аргумент владеет String независимо от дальнейшей жизни Window.

Для результата r2s compiler уже выдавал classic очистку .t после копирования
в аргумент. Теперь эта инструкция получает роль223. Builder проверяет связь
с предыдущим FP String store, карты обоих слотов, запрет literal String как
nil source и отсутствие последующих чтений очищенной временной. После
публикации String в аргументе materializer исполняет обычную очистку caller.

Обе роли проверяются до выделений в Sys module builder; orphan/перекрытые
роли вне единственного variadic диапазона отказываются. Сам callee layout,
тип String tail, количество/порядок слотов, result binding и borrowed-result
политика продолжают проверяться существующими Sys gateways. Нового result
ABI, общего доступа к полям ADT и сериализованных variadic записей нет.
Classic Dis bytes и 32-битный код не меняются.

## Воспроизведение

```
python3 utils/tests/native64-format-fields.py --limbo /path/to/limbo
```

Исполняются настоящие Limbo r2s из awm.b, String операции из string.c,
Sys_sprint64/Program gateways из runt.c и lib9 formatter/quoting. Module
entry связывается через реальный linkmod. Sys registration service ограничен
fixture; output String создаёт настоящий производитель.

* 300 C/JIT запусков: FP/MP Window.tag, nested r2s, fresh/inherited stack
  result, три quantum, ASCII/пробелы/Unicode/nil tag, nil Window и отказ
  выделения в форматтере. Проверяются текст, кавычки, сохранность Window,
  caller guards, предыдущий результат и полный retirement владельцев.
* На входе Sys formatter дополнительно проверяется точный tag и refcount3:
  fixture + Window + подготовленный аргумент. У временной r2s проверяется
  refcount1: её удерживает уже только кадр аргументов.
* Независимый classic oracle выявляет 36 подмен field/clear записей и ещё
  50 подмен variadic metadata; signatures, maps и classic bytes сверяются.
* 19 повреждений opcode/mode/span/map/clear pairing/literal/orphan recipes
  отклоняются до выделений с сохранением REG.
* Все 50/51 точек отказа конструктора C/JIT дают полный rollback.
* Три активных контроля: другое поле вместо tag, сохранение caller temporary
  через self-copy, отсутствие owner root proof. Требуется смысловой отказ
  проверки; sanitizer crash не засчитывается.

Локально прошли UBSan и ASan+UBSan (`detect_leaks=0`, владельцы проверяются
явными счётчиками), все активные контроли, coordinate formatter180 и local
geometry240. Чистая amd64 console-сборка проходит pair boot и настоящий
Arg/emuinit boot324. Gate включён в native64-port.py и оба пути CI.
Arm64/rv64 выполнение и Android UI локально не проверялись.
