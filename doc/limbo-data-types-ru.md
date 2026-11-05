# Типы данных Limbo и граница с C

Примеры рассчитаны на обычный компилятор Limbo из Inferno. В конце есть отдельная карта поддержки экспериментального Dis64. Команды запускаются из корня репозитория. В примерах `nil` означает пустое значение ссылочного типа, а `len` — длину строки, массива или списка.

## 1. Числа, строки и составные значения

Сохраните пример как `types.b`. В Inferno команда `types.dis` вызовет `init` по интерфейсу `Command`.

```limbo
implement Types;
include "sys.m";
include "draw.m";

Types: module {
    init: fn(ctxt: ref Draw->Context, args: list of string);
};

Point: adt {
    x, y: int;
};

Row: adt {
    name: string;
    points: array of Point;
};

Scene: adt {
    rows: array of Row;
    origin: Point;
    selected: ref Point;
};

Value: adt {
    name: string;
    pick {
    Count =>
        n: int;
    Text =>
        text: string;
    Shape =>
        points: array of Point;
    Empty =>
    }
};

describe(v: ref Value): string
{
    pick p := v {
    Count => if(p.n > 0) return p.name;
    Text => return p.text;
    Shape => if(len p.points > 0) return p.name;
    Empty => return "empty";
    }
    return "unknown";
}

init(nil: ref Draw->Context, nil: list of string)
{
    sys := load Sys Sys->PATH;
    b: byte = byte 255;
    i: int = -42;
    wide: big = big 1 << 40;
    r: real = 1.25;
    s := "Привет";
    utf := array of byte s;
    words := s :: "Limbo" :: nil;
    (x, y) := (3, 4);

    p := Point(x, y);
    q := p;
    q.x = 9;
    shared := ref Point(7, 8);
    alias := shared;
    alias.x = 10;

    cells := array[] of {Point(1, 2), p};
    refs := array[] of {shared, ref Point(0, 0)};
    matrix := array[] of {array[] of {1, 2}, array[] of {3}};
    view := cells[0:1];
    view[0] = Point(5, 6);
    scene := Scene(array[] of {Row("first", cells)}, Point(0, 0), refs[0]);

    variants := array[3] of ref Value;
    variants[0] = ref Value.Count("items", 2);
    variants[1] = ref Value.Text("label", string utf);
    variants[2] = ref Value.Shape("shape", scene.rows[0].points);

    sys->print("%d %d %bd %g\n", int b, i, wide, r);
    sys->print("%s: %d runes, %d bytes\n", hd words, len s, len utf);
    sys->print("copy=%d/%d shared=%d view=%d matrix=%d\n",
        p.x, q.x, scene.selected.x, cells[0].x, matrix[1][0]);
    for(j := 0; j < len variants; j++)
        sys->print("%s\n", describe(variants[j]));
}
```

```sh
limbo -I module -o types.dis types.b
limbo -I module -S -o types.s types.b
```

В строке `copy` ожидаются `3/9`, `shared=10`, `view=5` и `matrix=3`. У слова «Привет» шесть рун и двенадцать байтов UTF-8.

| Тип | Что хранится и что копируется |
|---|---|
| `byte`, `int`, `big`, `real` | Беззнаковое 8-битное число, знаковые 32- и 64-битные целые и 64-битное вещественное соответственно. Для преобразований используйте явные `int b`, `big i`, `real i`. |
| `string` | Последовательность рун. Индекс `s[i]` даёт код руны. Преобразование `array of byte s` даёт байты UTF-8, а `string utf` декодирует их обратно. |
| `(int, string)` | Кортеж из значений полей. `(n, text) := pair` разбирает его. Ссылочные поля при копировании сохраняют связь с тем же объектом. |
| `list of T` | Список с пустым хвостом `nil`. `head :: tail` создаёт новую голову. `hd` и `tl` применяйте после проверки на `nil`. |
| `array of T` | Ссылка на массив. Присваивание массива разделяет хранилище. Срез `a[lo:hi]` разделяет элементы исходного массива, правая граница исключается. |
| `Point`, `ref Point` | Присваивание ADT по значению копирует поля. Присваивание `ref Point` копирует ссылку, поэтому изменение поля видно через обе переменные. |
| `array of Point`, `array of ref Point` | В первом массиве элементом служит значение ADT. Во втором элемент — ссылка, которую можно заменить или оставить `nil`. |
| `array of array of int` | Внешний массив хранит ссылки на строки. У строк могут быть разные длины, как в `matrix`. |
| `Row`, `Scene` | ADT может содержать массивы и другие ADT. Копирование `Row` копирует поле `name` и ссылку `points`, поэтому элементы `points` остаются общими. |
| `ref Value` | ADT с дискриминатором `pick`. `Value.Count` и `Value.Text` содержат разные поля. Оператор `pick` связывает переменную с конкретным вариантом. |

Обычный ADT можно вкладывать по значению. Для рекурсивной структуры используйте ссылку. Если поле может замыкать цикл, объявляйте его с `cyclic`, как в `module/json.m`. Варианты `pick` хорошо подходят для дерева, где один узел хранит число, другой строку, третий массив дочерних узлов.

## 2. Каналы, функции, модули и специальные типы

Следующие объявления и выражения можно добавить в модуль. Функции объявляются на уровне модуля.

```limbo
addone(n: int): int
{
    return n + 1;
}

sender(out: chan of int)
{
    out <-= 42;
}

apply(f: ref fn(n: int): int, n: int): int
{
    return f(n);
}

channeltest(): int
{
    c := chan of int;
    spawn sender(c);
    n := <-c;
    f: ref fn(n: int): int = addone;
    return apply(f, n);
}

Eighths: type fixed(0.125);
BadIndex: exception(int);

Box: adt[T] {
    item: T;
};

specials(i: int): (real, string, int)
{
    x := Eighths(1.25);
    y := Eighths(0.5);
    b := Box[string]("hello");
    n := 0;
    {
        if(i < 0)
            raise BadIndex(i);
        n = i;
    } exception e {
        BadIndex => (n) = e;
        * => raise;
    }
    return (real (x+y), b.item, n);
}
```

- `chan of T` передаёт значение типа `T`. В примере отправитель работает в отдельном процессе, а получатель возвращает 43. Канал может передавать кортеж, массив или ссылку на ADT.
- `ref fn(...): T` хранит ссылку на функцию. `fn(...)` без результата применяется для процедур.
- `Sys` в `load Sys Sys->PATH` задаёт интерфейс, `Sys->PATH` даёт имя `$Sys`. Переменная `sys` хранит ссылку на связанный модуль.
- `nil` получает тип из контекста. Для проверки условия используется `int`, где ноль ложен. Флаги часто объявляют как `con 1<<iota`.
- `fixed` задаёт число с фиксированным масштабом. Масштаб входит в объявление типа. На границе C используйте сгенерированное представление и явно документируйте масштаб.
- `exception` объявляет именованное исключение с параметрами. Передача идёт через `raise` и обработчик `exception`.
- `adt[T]` и `fn[T]` вводят параметр типа. Например, `module/tables.m` описывает `Table[T]`, а `module/lists.m` — обобщённые функции над списками. C-интерфейс лучше описывать с конкретными типами, для которых Limbo выдаёт фрейм и карту.

## 3. Один интерфейс для Limbo и C

Сохраните файл как `module/typedemo.m`.

```limbo
Typedemo: module {
    PATH: con "$Typedemo";

    Point: adt { x, y: int; };
    Row: adt {
        name: string;
        points: array of Point;
    };
    Value: adt {
        pick {
        Number => n: int;
        Text => s: string;
        Rows => rows: array of Row;
        }
    };

    plusone: fn(n: int): int;
    greeting: fn(): string;
    bytes: fn(): array of byte;
    firstx: fn(points: array of Point): int;
    measure: fn(value: ref Value): int;
    firstcell: fn(rows: array of array of int): int;
};
```

```sh
limbo -a -I module module/typedemo.m > typedemo.h
limbo -t Typedemo -I module module/typedemo.m > typedemomod.h
limbo -T Typedemo -I module module/typedemo.m
```

`-a` выдаёт C-типы и фреймы, `-t` — таблицу экспорта с именами, хэшами и картами, `-T` — заготовки функций.

Сохраните реализацию ниже как `libinterp/typedemo.c`. Добавьте `typedemo.$O` к объектам в `libinterp/mkfile` и добавьте туда эти правила. Команды под целями начинаются табуляцией.

```makefile
typedemo.$O: typedemo.h typedemomod.h

typedemo.h:D: ../module/typedemo.m
	limbo -a -I../module ../module/typedemo.m > $target.tmp && mv $target.tmp $target

typedemomod.h:D: ../module/typedemo.m
	limbo -t Typedemo -I../module ../module/typedemo.m > $target.tmp && mv $target.tmp $target
```

Добавьте `typedemo` в секцию `mod` выбранной конфигурации `emu` или `emu-g` и пересоберите эту конфигурацию. `mkdevc` добавит вызов `typedemomodinit()`. Внутреннее имя при регистрации должно совпадать с `PATH`.

```c
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "typedemo.h"
#include "typedemomod.h"

void
Typedemo_plusone(void *fp)
{
    F_Typedemo_plusone *f = fp;
    *f->ret = (WORD)((UWORD)f->n + 1U);
}

void
Typedemo_greeting(void *fp)
{
    F_Typedemo_greeting *f = fp;
    retstr("hello", f->ret);
}

void
Typedemo_bytes(void *fp)
{
    F_Typedemo_bytes *f = fp;
    Array *a, *old;
    char bytes[] = "abc";

    a = mem2array(bytes, sizeof(bytes)-1);
    old = *f->ret;
    *f->ret = a;
    destroy(old);
}

void
Typedemo_firstx(void *fp)
{
    F_Typedemo_firstx *f = fp;
    Typedemo_Point p;

    *f->ret = 0;
    if(f->points == H || f->points->len == 0)
        return;
    memmove(&p, f->points->data, sizeof(p));
    *f->ret = p.x;
}

void
Typedemo_measure(void *fp)
{
    F_Typedemo_measure *f = fp;
    Typedemo_Value *v = f->value;
    Typedemo_Row row;
    Array *rows;

    *f->ret = 0;
    if(v == H)
        return;
    switch(v->pick){
    case Typedemo_Value_Number:
        *f->ret = v->u.Number.n;
        break;
    case Typedemo_Value_Text:
        if(v->u.Text.s != H)
            *f->ret = v->u.Text.s->len;
        break;
    case Typedemo_Value_Rows:
        rows = v->u.Rows.rows;
        if(rows == H || rows->len == 0)
            break;
        memmove(&row, rows->data, sizeof(row));
        if(row.points != H)
            *f->ret = row.points->len;
        break;
    }
}

void
Typedemo_firstcell(void *fp)
{
    F_Typedemo_firstcell *f = fp;
    Array *row;

    *f->ret = 0;
    if(f->rows == H || f->rows->len == 0)
        return;
    memmove(&row, f->rows->data, sizeof(row));
    if(row != H && row->len != 0)
        memmove(f->ret, row->data, sizeof(*f->ret));
}

void
typedemomodinit(void)
{
    builtinmod("$Typedemo", Typedemomodtab, Typedemomodlen);
}
```

После встраивания скомпилируйте следующий файл как `calltypedemo.b` и запустите его в Inferno с этим встроенным модулем.

```limbo
implement Calltypedemo;
include "sys.m";
include "draw.m";
include "typedemo.m";

Calltypedemo: module {
    init: fn(ctxt: ref Draw->Context, args: list of string);
};

init(nil: ref Draw->Context, nil: list of string)
{
    sys := load Sys Sys->PATH;
    demo := load Typedemo Typedemo->PATH;
    if(demo == nil){
        sys->print("load Typedemo: %r\n");
        return;
    }
    Point, Row, Value: import demo;
    points := array[] of {Point(7, 8), Point(9, 10)};
    rows := array[] of {Row("first", points)};
    matrix := array[] of {array[] of {11, 12}, array[] of {13}};
    sys->print("%d %s %s %d %d %d\n",
        demo->plusone(2), demo->greeting(), string demo->bytes(),
        demo->firstx(points), demo->measure(ref Value.Rows(rows)),
        demo->firstcell(matrix));
}
```

```sh
limbo -I module -o calltypedemo.dis calltypedemo.b
```

Ожидаемый вывод — `3 hello abc 7 2 11`.

### Как читать составные объекты в C

| Объект Limbo | Что использовать в C |
|---|---|
| `string` | `String *`, а для преобразования к UTF-8 — `string2c`. Для результата используйте `retstr` или `retnstr` с известной длиной. |
| `array of Point` | `Array *`. Шаг элемента задаёт `a->t->size`. Сгенерированный `Typedemo_Point` описывает значение элемента для согласованного ABI. |
| `array of ref Point` | `Array *` с элементами-ссылками. Перед разыменованием проверьте, что элемент отличается от `H`. |
| `array of array of int` | Во внешнем `Array` каждый элемент — `Array *`. Проверяйте границу внешнего массива, наличие строки, затем границу внутреннего массива. |
| `array of Row` | В элементе лежат поля `Row`, включая ссылки на строку и массив. Копирование байтов во временную C-структуру даёт заимствованные ссылки, пока исходный владелец жив. |
| `ref Value` | Выбирайте структуру варианта по тегу, который сгенерировал Limbo. При создании берите дескриптор конкретного варианта, чтобы GC видел его собственные ссылочные поля. |
| Список, канал, модуль | Используйте типы рантайма `List`, `Channel` и `Modlink` и операции VM. Раскладка этих объектов определяется рантаймом. |

Если C создаёт массив через `heaparray`, передавайте дескриптор **элемента**, включая его карту ссылок. Для массива строк и массива массивов карта элемента содержит ссылку. Для массива ADT она описывает ссылочные поля внутри каждого элемента. Для ADT с вложенным ADT по значению карта учитывает и его поля.

Для записи заимствованной ссылки в слот сначала удержите новый объект и отметьте его для GC через действующий механизм рантайма. Затем опубликуйте новое значение и отпустите старое. Для свежего объекта можно передать уже принадлежащую ссылку слоту. Между созданием и публикацией учитывайте вызовы, которые могут запустить GC или выбросить исключение. `H` — пустое значение VM, `nil` — нулевой C-указатель. За примерами смотрите `retstr` в `libinterp/runt.c`, `newmp` и `heaparray` в `libinterp/heap.c`.

## 4. Что переносится в Dis64

Обычный Dis формирует таблицу импортов из `.m` и связывает группу по имени и хэшу. При переносе в Dis64 нужно отдельно описать сериализуемые значения, нативную раскладку фреймов и владение ссылками. Одинаковый хэш интерфейса позволяет проверить тип Limbo. Размер слота, карта GC и ABI результата проверяются отдельно.

| Область | Состояние в экспериментальной ветке |
|---|---|
| Числа | Есть ограниченный компилятор `limbo -64` и скалярные исполняемые профили. Поддержка зависит от операции, а также от профиля допуска. |
| Строки | Пул Rune32 хранит имена и строки как данные формата. Это отдельный слой от создания `String *` и передачи управляемой строки через импорт. |
| Модули | Есть ограниченная парная загрузка и явный сценарий `$Sys` для `millisec`. Обычный набор импортов AWM шире. |
| Массивы, ADT, варианты и вложенность | Обычные примеры выше компилируются в Dis. Для исполнения Dis64 нужны согласованные карты, операции, правила владения и допуск конкретных профилей. |
| Вызовы C | Для выбранных интерфейсов ветка генерирует native64-декларации через `limbo -H`. Для конкретной функции проверяйте её регистрацию и профиль запуска. |

Разметку обычных C-фреймов из `-a` следует использовать с соответствующим ABI обычного рантайма. Для ARM64 сначала сверяйте нативные декларации, карты и тесты профиля. Проверки генерации JIT, проверка формата независимым reader FlashVM и успешный запуск программы на конкретной архитектуре дают разные результаты.

Дополнительные материалы — [справка Limbo](../man/1/limbo), [дополнения к языку](limbo/addendum.ms), [примеры вариантов в JSON](../module/json.m), [профиль Dis64](dis64-pilot.md) и статьи о разработке модулей Limbo на C ([часть 1](https://habr.com/ru/articles/118665/), [часть 2](https://habr.com/ru/articles/118696/)).
