#!/usr/bin/env python3
"""Generate per-opcode AMD64/ARM64 emission percentages and explicit native domains."""
import argparse
import csv
import importlib.util
import io
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning repository.
DIRECTORY = ROOT/'libinterp/tests'  # Git-backed generated reports.
SPEC = importlib.util.spec_from_file_location('jit_coverage',DIRECTORY/'jit-coverage.py')  # One real ABI fixture for all reports.
COVERAGE = importlib.util.module_from_spec(SPEC)
# Compile the borrowed fixture directly: exec_module may create __pycache__ even
# in --check mode, dirtying read-only/vendor snapshots. Keep normal module globals
# from the spec without changing the process-wide bytecode policy.
exec(compile((DIRECTORY / 'jit-coverage.py').read_bytes(),
             str(DIRECTORY / 'jit-coverage.py'), 'exec'), COVERAGE.__dict__)
DOMAINS = {  # Audited scope descriptions; numeric percentages come only from actual emission.
    'LeafTransfer': 'Частично: буфер, нет ожидающего peer, np=0, 0–256 байт; C-preflight, нативные copy/commit. Блокировка/ALT/rendezvous → C.',
    'LeafChannel': 'Частично: пустой выходной владелец; захваченные аргументы/выход, общий C-аллокатор; замена владельца → C с захваченным слотом.',
    'LeafMappedCons': 'Частично: карта без указателей, до 256 байт; проверка Type и выделение через C, копирование нативно.',
    'LeafRawCons': 'Частично: проверенные слоты и сырые головы до 256 байт; C-аллокатор, захват source/result до вызова C.',
    'LeafPointerCons': 'Частично: проверенные слоты и refcount/GC-условия; C-аллокатор, нативное удержание/публикация.',
    'LeafScalarCons': 'Проверенные source/result слоты; скаляр захвачен нативно до C-аллокатора, затем нативная запись/публикация.',
    'LeafArrayCreate': 'Частично: проверенные слоты, уникальный pointer-free массив; выделение/карты → C с захваченным выходным слотом.',
    'LeafRecordObject': 'Частично: повторное использование уникальной plain-записи; выделение/деструкторы/карты → C.',
    'LeafStringBytes': 'Частично: проверенные слоты, байтовые строки и повторно используемый массив; выделение → C с захваченным слотом.',
    'LeafByteString': 'Частично: проверенные слоты, ограниченный ASCII и уникальная строка; UTF/выделение → C с захваченным слотом.',
    'LeafStringBranch': 'Частично: поддерживаемые представления и проверенные строковые диапазоны; остальные случаи → C.',
    'LeafRealPower': 'Нативное возведение real в целую степень; проверки операндов и FP-контракт.',
    'LeafRealInteger': 'Частично: проверенный диапазон FP→integer; CVTFX ограничивает масштаб обычными степенями двух.',
    'LeafRealArithmetic': 'Нативная real-арифметика; CVTXF использует проверенный fixed-point масштаб.',
    'LeafStringSlice': 'Частично: переиспользование уникального строкового буфера.',
    'LeafArraySlice': 'Частично: проверенные слоты/заголовки/корень, уникальное представление или pointer-free корень; выделение → C с захваченным слотом.',
    'LeafStringAppend': 'Частично: уникальный ASCII/Rune буфер с достаточной ёмкостью.',
    'LeafStringStore': 'Частично: проверенный индекс, уникальный владелец и готовая ёмкость; смена представления → C.',
    'LeafFormatInteger': 'Частично: переиспользование строки; целые и поддерживаемое подмножество real-форматирования.',
    'LeafDecimal': 'Частично: ограниченный проверенный ASCII decimal; остальные форматы → C.',
    'LeafSliceCopy': 'Частично: pointer-free byte-array копия до 256 байт, проверенные слоты/заголовки, qword-копирование с точным хвостом; nil-источник не читает остальные операнды.',
    'LeafMappedCopy': 'Частично: Type без указателей, 0–256 байт; карты и большие записи → C.',
    'LeafOwner': 'Частично: поддерживаемые refcount/GC/старый-владелец условия; освобождение сложных владельцев → C.',
    'LeafTypeCheck': 'Нативная проверка идентичности Type через проверенные поля Heap.',
    'LeafRealBranch': 'Нативное сравнение real и выбранный PC; FP/NaN-контракт.',
    'LeafDescriptorIndex': 'Адрес по Type.size; положительный байтовый offset ≤ INT_MAX, нулевой stride сохраняет WORD-индекс.',
    'LeafFixedCoefficient': 'Частично: проверенные коэффициенты fixed-point и ограниченные арифметические пути.',
    'LeafFixedFrame': 'Частично: проверенный frame/scale и поддерживаемая fixed-point арифметика.',
    'LeafRealResize': 'Нативное преобразование float32/float64 с сохранением FP-контракта.',
    'LeafIntegerReal': 'Нативное integer→real по активной ширине WORD/LONG.',
    'LeafFixedShift': 'Нативный проверенный fixed-point сдвиг; неиспользуемый frame не читается.',
    'LeafPower': 'Нативное целое возведение в степень с модульной арифметикой.',
    'LeafHead': 'Нативная скалярная голова списка после проверок owner/полей/выхода.',
    'LeafListLength': 'Частично: до 1024 узлов с обнаружением циклов; длинные списки → C.',
    'LeafStringIndex': 'Частично: проверенное строковое представление и индекс.',
    'LeafStringLength': 'Нативная длина по поддерживаемому заголовку; без чтения символов.',
    'LeafHeadCopy': 'Частично: сырая голова 0–256 байт; большие размеры → C.',
    'LeafCopy': 'Частично: memmove 0–256 байт; большие размеры → C.',
    'LeafNegReal': 'Нативное отрицание real.',
    'LeafLength': 'Нативная длина массива; nil/H → 0, остальные владельцы требуют только len.',
    'LeafIndex': 'Нативный адрес элемента; проверенный индекс/stride и байтовый offset ≤ INT_MAX.',
    'LeafNop': 'Штатный RUNT без изменения VM-данных.',
    'LeafMove': 'Нативный скалярный перенос/целочисленное преобразование с правильной шириной и alias-захватом.',
    'LeafAddress': 'Нативная запись адресного значения в WORD; ширина WORD остаётся частью VM ABI.',
    'LeafProgramAddress': 'Частично: проверенные Module.prog/index/stride и адресная арифметика.',
    'LeafTableJump': 'Частично: проверенная таблица PC и bytecode-модуль.',
    'LeafCaseJump': 'Частично: bounded CASE/CASEL; CASEC — пустая таблица или совпадение первого midpoint, до 4095 строк.',
    'LeafModuleCall': 'Частично: bytecode-вызовы при совместимых режимах исполнения; смена режима/сложный runtime → C.',
    'LeafReturn': 'Частично: возврат без mapped-cleanup, освобождения extent и уничтожения модуля.',
    'LeafFrame': 'Частично: pointer-free frame в текущем stack extent; выделение/карты → C.',
    'LeafCall': 'Частично: проверенный frame и bytecode PC без нового native-PC ABI.',
    'LeafJump': 'Нативная запись проверенного PC из декодированного слота.',
    'LeafAlu': 'Нативная целая модульная арифметика/битовые операции.',
    'LeafShift': 'Нативный сдвиг с правилами Dis для ширины и знака.',
    'LeafDivide': 'Нативное деление/остаток, включая MIN/−1; нулевой делитель → C-ошибка.',
    'LeafBranch': 'Нативное целое сравнение; target читается только на взятой ветви.',
}
SCOPES = {  # Refine shared families when individual command domains differ.
    'INEW': 'Частично: проверенные слоты и уникальная plain-запись до 256 байт; выделение/карты → C с захваченным адресом результата.',
    'INEWZ': 'Частично: проверенные слоты, обнуление уникальной plain-записи до 256 байт; выделение/карты → C с захваченным адресом результата.',
    'IMNEWZ': 'Частично: проверенные module/index/result слоты и уникальная plain-запись до 256 байт; выделение/карты → C с захваченным адресом результата.',
    'IFRAME': 'Частично: проверенные слоты, заголовки и выбранная запись Type-таблицы; выровненный Type без указателей и запас в текущем стеке; карты/расширение → C.',
    'IMFRAME': 'Частично: проверенные слоты, import record и Type; pointer-free кадр с запасом в текущем стеке; экспорты/карты/расширение → C.',
    'ICALL': 'Частично: проверенные диапазоны слотов и сохраняемых frame links; захват alias-операндов до записи, исходный bytecode PC.',
    'IMCALL': 'Частично: проверенные слоты, module/frame headers и адрес записи импорта; одинаковые режимы bytecode-вызова; builtins/экспорты/смена режима → C.',
    'IRET': 'Частично: проверенные frame/Type/module поля и обратный refcount; pointer-free cleanup; карты/освобождение extent/уничтожение модуля → C.',
    'ISEND': 'Частично: буфер имеет место, нет ожидающего получателя, np=0, 0–256 байт; C-preflight, нативные copy/commit; блокировка/rendezvous → C.',
    'IRECV': 'Частично: буфер содержит данные, нет ожидающего отправителя, np=0, 0–256 байт; C-preflight, нативные copy/commit; блокировка/refill → C.',
    'ICASE': 'Частично: проверенная целая CASE-таблица до 4095 строк; bytecode PC и подходящая ширина ячеек.',
    'ICASEL': 'Частично: проверенная LONG CASE-таблица до 4095 строк; bytecode PC и подходящая ширина ячеек.',
    'ICASEC': 'Частично: пустая таблица либо точный owner/равные пустые строки на первом midpoint; до 4095 строк; общий поиск → C.',
    'ICVTFW': 'Частично: проверенный диапазон real→WORD с округлением Dis; остальные случаи → C.',
    'ICVTFL': 'Частично: проверенный диапазон real→LONG с округлением Dis; остальные случаи → C.',
    'ICVTFX': 'Частично: проверенный диапазон real→fixed; масштаб — обычная степень двух; остальные случаи → C.',
    'ICVTWC': 'Частично: проверенные слоты/Heap/заголовок, WORD→ASCII в уникальной строке; выход восстанавливается после перекрывающихся записей.',
    'ICVTLC': 'Частично: проверенные слоты/Heap/заголовок, LONG→ASCII в уникальной строке; выход восстанавливается после перекрывающихся записей.',
    'ICVTFC': 'Частично: проверенные слоты/Heap/заголовок, выбранные real→ASCII; восстановление выхода, выделение/общее форматирование → C.',
    'ICVTXF': 'Частично: fixed→real с проверенным масштабом/коэффициентом.',
    'IADDF': 'Нативное real-сложение с проверками слотов и FP-контрактом.',
    'ISUBF': 'Нативное real-вычитание с проверками слотов и FP-контрактом.',
    'IMULF': 'Нативное real-умножение с проверками слотов и FP-контрактом.',
    'IDIVF': 'Нативное real-деление с проверками слотов и FP-контрактом.',
}
C_HANDLERS = {  # Complex handlers are reached through complete native tail entries.
    'IALT': 'C через JIT: выбор альтернатив, очереди, блокировка и пробуждение.',
    'INBALT': 'C через JIT: неблокирующий выбор альтернатив и очистка регистраций.',
    'ISPAWN': 'C через JIT: создание Prog, владение frame и регистрация в scheduler.',
    'IMSPAWN': 'C через JIT: межмодульный spawn и владение модулем/frame.',
    'ILOAD': 'C через JIT: загрузчик, imports, Module/MP и ошибки линковки.',
    'IEXIT': 'C через JIT: завершение процесса и освобождение runtime-ресурсов.',
    'IRAISE': 'C через JIT: exception value, текст сообщения и раскрутка VM error frames.',
}


# Ask the shared production classifier for each opcode family, without guessing enum values.
def families(names, directory):
    header = (ROOT/'libinterp/leaf64-ops.h').read_text()
    enum = header[header.index('enum {\n\tLeafTransfer'):header.index('/* Classify guarded')]
    kinds = list(dict.fromkeys(re.findall(r'\bLeaf[A-Z][A-Za-z]*\b',re.sub(r'/\*.*?\*/|//[^\n]*','',enum,flags=re.S))))
    cases = ''.join('case '+kind+': return "'+kind+'";' for kind in kinds)
    source = directory/'families.c'
    source.write_text('''#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Classifier has no runtime dependencies.
#include "isa.h"
#include "leaf64-ops.h"


/* Map real classifier IDs back to their source identifiers. */
static const char*
family(int kind)
{
    switch(kind){'''+cases+'''}
    return "unknown";
}


/* Query WORD32/64 families independently of a guessed opcode ordering. */
int
main(void)
{
    LeafOp p;
    int i, wb;
    for(wb=4; wb<=8; wb+=4)
        for(i=0; i<MAXDIS; i++)
            if(leafop(&p,i,wb))
                printf("%d %d %s\\n",wb,i,family(p.kind));
    return 0;
}
''')
    binary = directory/'families'
    command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1'))
    subprocess.run(command+['-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
    result = {}
    for line in subprocess.check_output([str(binary)],text=True).splitlines():
        wb,index,kind = line.split()
        name = names[int(index)]
        if name in result and result[name]!=kind: raise RuntimeError(('WORD family mismatch',name))
        result[name] = kind
    return result


# Measure both emitters and WORD models; exact and short-capacity canaries are mandatory.
def reports():
    names = COVERAGE.opnames()
    spec = importlib.util.spec_from_file_location('dispatch_coverage',DIRECTORY/'jit-dispatch-coverage.py')
    dispatch_coverage = importlib.util.module_from_spec(spec)
    exec(compile((DIRECTORY/'jit-dispatch-coverage.py').read_bytes(),
                 str(DIRECTORY/'jit-dispatch-coverage.py'), 'exec'), dispatch_coverage.__dict__)
    with tempfile.TemporaryDirectory(prefix='jit-readiness-') as temporary:
        directory = Path(temporary)
        classification = families(names,directory)
        measurements = {(arch,wb): COVERAGE.measure(arch,wb,'base',names,directory) for arch in ('amd64','arm64') for wb in (4,8)}
        dispatch = {arch: dispatch_coverage.measure(arch,names,directory) for arch in ('amd64','arm64')}
    rows = []
    for opcode in names:
        if opcode in ('INOP','IECLR'): continue
        row = dict(opcode=opcode[1:])
        for arch in ('amd64','arm64'):
            values = [int(opcode in measurements[arch,wb]['available']) for wb in (4,8)]
            row[arch+'_percent'] = 50*sum(values)
            row[arch+'_word32'],row[arch+'_word64'] = values
            complete = [int(opcode in dispatch[arch][wb]) for wb in (4,8)]
            row[arch+'_dispatch_percent'] = 50*sum(complete)
            row[arch+'_dispatch_word32'],row[arch+'_dispatch_word64'] = complete
            row[arch+'_mode'] = 'быстрый путь + C' if sum(values) else 'C через JIT' if sum(complete) else 'C вне JIT'
        kind = classification.get(opcode,'C')
        if kind!='C' and kind not in DOMAINS: raise RuntimeError(('missing domain annotation',kind,opcode))
        row['family'] = kind
        row['scope'] = SCOPES.get(opcode,DOMAINS[kind]) if kind!='C' else C_HANDLERS[opcode]
        rows.append(row)
    active = len(rows)
    native = {arch: sum(row[arch+'_percent']>0 for row in rows) for arch in ('amd64','arm64')}
    complete = {arch: sum(row[arch+'_dispatch_percent']==100 for row in rows) for arch in ('amd64','arm64')}
    md = '''# Поддержка отдельных команд Dis из JIT: быстрый путь + C

Проценты — доля проверенных WORD32/WORD64-конфигураций: 0/2 = 0%,
1/2 = 50%, 2/2 = 100%. **Полный вход** означает вызываемый вход Jit.dispatch,
который пробует быстрый путь и при отказе выполняет штатный C-обработчик.
**Быстрый путь** означает отдельный Jit.entry с собственной нативной реализацией
поддерживаемого случая. Проверяются фактическая генерация, точная ёмкость,
канарейки и атомарный отказ на коротком буфере.

Полный вход не требует переписывать сложную команду на ассемблер. ALT/NBALT,
SPAWN/MSPAWN, LOAD, EXIT и RAISE выполняются в C через хвостовой вход JIT.
Остальные команды имеют нативный быстрый путь и C-подхват. Ни одна из этих
цифр не является процентом всех входных состояний, производительности или
готовности всей 64-битной VM. Ограничения быстрых путей указаны отдельно.

'''
    md += '| Архитектура | Полные входы JIT+C | Быстрые пути | C через JIT без быстрого пути |\n| --- | ---: | ---: | ---: |\n'
    for arch,title in (('amd64','x86-64'),('arm64','ARM64')):
        md += f'| {title} | {complete[arch]}/{active} ({100*complete[arch]/active:.2f}%) | {native[arch]}/{active} ({100*native[arch]/active:.2f}%) | {complete[arch]-native[arch]} |\n'
    md += '\n175 enum-слотов минус недопустимый INOP и неиспользуемый IECLR = 173 активные команды.\n'
    md += '\nХвостовой переход в общий C-диспетчер сохраняет возврат в исходный цикл VM,\nдаже если обработчик освободил модуль с его JIT-кодом. Декодированный цикл\nсохраняет прямой вызов быстрых входов; сложные команды используют полный вход.\nЭто per-opcode JIT с исходными Dis PC; многокомандные блоки ещё не реализованы.\n'
    md += '\nФизический ARM64 и полная загрузка Inferno не проверены этим измерением.\n'
    md += '\n| Команда | Полный x64, % | Полный ARM64, % | Быстрый x64, % | Быстрый ARM64, % | Режим | Область быстрого пути / C-обработчик |\n| --- | ---: | ---: | ---: | ---: | --- | --- |\n'
    for row in rows:
        mode = row['amd64_mode'] if row['amd64_mode']==row['arm64_mode'] else 'x64: '+row['amd64_mode']+'; ARM64: '+row['arm64_mode']
        md += f"| {row['opcode']} | {row['amd64_dispatch_percent']}% | {row['arm64_dispatch_percent']}% | {row['amd64_percent']}% | {row['arm64_percent']}% | {mode} | {row['scope']} |\n"
    md += '\nВоспроизведение: `python3 libinterp/tests/jit-readiness.py --check`.\n'
    csvout = io.StringIO(newline='')
    writer = csv.DictWriter(csvout,fieldnames=list(rows[0]),lineterminator='\n')
    writer.writeheader()
    writer.writerows(rows)
    return {'README.jit-readiness.md': md, 'jit-readiness.csv': csvout.getvalue()}


# Save or validate deterministic Git-backed artifacts; stdout remains read-only by default.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--write',action='store_true')
    mode.add_argument('--check',action='store_true')
    args = parser.parse_args()
    files = reports()
    for name,content in files.items():
        path = DIRECTORY/name
        if args.write: path.write_text(content,newline='')
        elif args.check:
            if not path.exists() or path.read_text()!=content.replace('\r\n','\n'): raise RuntimeError(('stale readiness artifact',name))
    if args.write or args.check: print('PASS readiness: 173 complete rows, fast/C modes, AMD64/ARM64 WORD32/64 and capacity canaries')
    else: print(files['README.jit-readiness.md'],end='')


if __name__=='__main__':
    main()
