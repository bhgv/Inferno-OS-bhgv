#!/usr/bin/env python3
"""Run the original Sh lexer and parser with generic array field probes in C and JIT."""
import argparse
import os
import re
import runpy
import shlex
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = runpy.run_path(str(ROOT / 'utils/tests/native64-record-fields.py'))
G, CMD, E = BASE['G'], BASE['CMD'], BASE['E']
EXTRACT = CMD['NS']['S']['FILES']['FILES']['function']
INTERFACES = 'include "sys.m";include "draw.m";include "bufio.m";include "string.m";include "sh.m";'
LEXER_EXPORTS = ('read', 'scan', 'token', 'parse')
ARRAY_EXPORTS = ('arrays', 'report', 'buildstring', 'buildfile', 'parse_result',
                 'fieldpath', 'rootarrays', 'listarg', 'lenarg')


def source():
    sh = (ROOT / 'appl/cmd/sh/sh.b').read_text()
    records = ''.join(re.search(r'(?m)^' + n + r':\s*adt\s*\{.*?^\};', sh, re.S)[0]
                      for n in ('YYSTYPE', 'YYLEX'))
    records += '''
Item:adt{text:string;value:int;};
Holder:adt{fd:ref Sys->FD;};
Envelope:adt{box:ref Holder;};
Pair:adt{text:string;fd:ref Sys->FD;};
Store:adt{
    words:array of int;
    texts:array of string;
    items:array of Item;
    i,n:int;
    text:string;
    payload:Item;
    lex:ref YYLEX;
    fd:ref Sys->FD;
    node:ref Node;
    problem:string;
    envelope:ref Envelope;
    pairs:array of Pair;
};
'''
    interface = INTERFACES + 'Node:type Sh->Node;Redir:type Sh->Redir;' + records + 'Lexer:module{'
    interface += ''.join(n + ':fn(l:ref YYLEX):int;' for n in LEXER_EXPORTS)
    interface += ''.join(n + ':fn(r:ref Store):int;' for n in ARRAY_EXPORTS) + 'setup:fn();};'
    constants = ''.join(re.search(r'(?m)^' + n + r':\s*con[^;]+;', sh)[0]
                        for n in ('DUP', 'REDIR', 'WORD', 'OP', 'END', 'ERROR', 'ANDAND', 'OROR',
                                  'NOTOKEN', 'YYPRIVATE', 'yydebug', 'OAPPEND', 'YYFLAG', 'YYLAST',
                                  'YYMAXDEPTH', 'YYEOFCODE', 'YYERRCODE'))
    constants += ''.join(n + ':con Sh->' + n + ';'
                         for n in ('GLOB', 'n_ASSIGN', 'n_LOCAL', 'n_COUNT', 'n_SQUASH', 'n_VAR',
                                   'n_BQ', 'n_BQ2', 'n_NOWAIT', 'n_ADJ', 'n_WORD', 'n_BLOCK',
                                   'n_PIPE', 'n_DUP', 'n_REDIR', 'n_CONCAT', 'n_LIST', 'n_SEQ'))
    tables = '\n'.join(re.search(r'(?m)^' + name + r' := array\[\] of \{.*?\};', sh, re.S)[0]
                       for name in ('yytok1', 'yytok2', 'yytok3', 'yyexca', 'yyact', 'yypact',
                                    'yypgo', 'yyr1', 'yyr2', 'yychk', 'yydef'))
    debug = re.search(r'(?m)^YYSys: module.*?^yystderr: ref YYSys->FD;', sh, re.S)[0]
    stack = re.search(r'(?m)^YYS:\s*adt\s*\{.*?^\};', sh, re.S)[0]
    blank = re.search(r'(?m)^blanklex:.*$', sh)[0]
    methods = '\n'.join(re.search(r'(?m)^' + re.escape(n) + r'\([^\n]+\n\{.*?^\}', sh, re.S)[0] for n in (
        'YYLEX.getc', 'YYLEX.ungetc', 'YYLEX.error', 'YYLEX.lex', 'readnum',
        'readfdassign', 'stderr', 'yytokname', 'yystatname', 'yylex1', 'mk', 'mkseq', 'yyparse',
        'YYLEX.initstring', 'YYLEX.initfile', 'doparse'))
    wrappers = '''
sys:Sys;
bufio:Bufio;
str:String;
yytoknames:array of string;
yystates:array of string;
saved:ref Store;
setup() {
    sys = load Sys "$Sys";
    bufio = load Bufio Bufio->PATH;
    str = load String String->PATH;
}
read(l:ref YYLEX):int { return l.getc(); }
scan(l:ref YYLEX):int { return l.lex(); }
token(l:ref YYLEX):int { return yylex1(l); }
parse(l:ref YYLEX):int { return yyparse(l); }
buildstring(r:ref Store):int { r.lex = YYLEX.initstring(r.text); return 0; }
buildfile(r:ref Store):int { r.lex = YYLEX.initfile(r.fd, r.text); return 0; }
parse_result(r:ref Store):int {
    (node, problem) := doparse(r.lex, "", r.n);
    r.node = node;
    r.problem = problem;
    if (problem != nil)
        return 1;
    return 0;
}
total(n:int, s:string):int {
    while (n < 0)
        n++;
    return n + len s;
}
arrays(r:ref Store):int {
    saved = r;
    n := r.n;
    t := r.text;
    r.words = array[r.n] of int;
    saved.words[r.i] = n;
    r.texts = array[r.n] of string;
    r.texts[r.i] = t;
    s := saved.texts[r.i];
    r.items = array[r.n] of Item;
    r.payload.text = s;
    r.payload.value = 43;
    r.items[r.i] = saved.payload;
    saved.payload = r.items[r.i];
    w := saved.items[r.i];
    r.words = saved.words[1:len r.words];
    copy := array[r.n] of int;
    copy[0:] = r.words;
    value := copy[0];
    return total(r.n, r.text) + w.value + value;
}
report(r:ref Store):int { return sys->fprint(stderr(), "%s %d", r.text, r.n); }
choosefd(fd:ref Sys->FD):ref Sys->FD { return fd; }
fieldpath(r:ref Store):int {
    r.fd = choosefd(r.envelope.box.fd);
    return r.n;
}
rootarrays(r:ref Store):int {
    r.pairs = array[r.n] of Pair;
    value:Pair;
    value.text = r.text;
    value.fd = r.fd;
    r.pairs[r.i] = value;
    copy := array[r.n] of Pair;
    copy[0:] = r.pairs;
    r.pairs = copy;
    value = copy[r.i];
    return len value.text;
}
lenarg(r:ref Store):int { return total(len r.pairs, r.text); }
listlen(items:list of string):int { return len items; }
listarg(r:ref Store):int { text := r.text; return listlen(text :: nil); }
'''
    return interface, 'implement Lexer;' + '\n'.join((interface, constants, tables, debug, stack, blank, wrappers, methods))


def generate(limbo, work):
    interface, limbo_source = source()
    decl = work / 'lexer.m'
    decl.write_text(interface)
    headers = {n: subprocess.check_output([str(limbo), '-H', '-M', n, '-I',
                                           str(ROOT / 'module'), str(decl)], text=True)
               for n in ('Lexer', 'Sys', 'Bufio', 'String')}
    # The Sys header supplies the shared FD definition.
    (work / 'lexer-native.h').write_text(re.sub(r'struct Sys_FD \{.*?\n\};\n', '', headers['Lexer'], count=1, flags=re.S))
    (work / 'sys-native64.h').write_text(headers['Sys'])
    signatures = {n: int(v, 16) for n, v in re.findall(
        r'^#define LIMBO_NATIVE64_SIGNATURE_Lexer_(\w+) (0x[0-9a-f]+)U', headers['Lexer'], re.M)}
    frames = {}
    for namespace in ('Sys', 'Bufio', 'String'):
        header = headers[namespace]
        for name, size in re.findall(r'^#define F_' + namespace + r'_(\w+)_size (\d+)', header, re.M):
            mapping = re.search(r'^#define F_' + namespace + '_' + name + r'_map \{([^}]*)\}', header, re.M)
            sig = re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_' + namespace + '_' + name
                            + r' (0x[0-9a-f]+)U', header, re.M)
            if sig:
                frames[(name.replace('Iobuf_', 'Iobuf.'), int(sig[1], 16))] = (
                    int(size), [int(v, 0) for v in mapping[1].split(',') if v])

    rows = G['generate'](limbo, work, limbo_source, 'Lexer', (33, 4, 3, '~0U'),
                         import_frames=frames, export_signatures=signatures,
                         source_label='unchanged Sh constructors, lexer and parser with generic ADT probes')
    code = {r['name']: r['code'] for r in rows}
    assert any(c[5] == 80 and (c[1] >> 3 & 7) in (4, 5) for c in code['getc'])
    assert {c[5] for c in code['arrays']} >= {33, 35, 65, 79, 80}
    assert sum(c[5] == 34 for c in code['report']) == 2
    assert {c[5] for c in code['yyparse']} >= {35, 36, 37, 65, 79, 80, 154}
    assert {c[5] for c in code['initstring']} >= {38, 39}
    assert {c[5] for c in code['initfile']} >= {38, 39, 41}
    assert any(c[5] == 42 for c in code['doparse'])
    assert {c[5] for c in code['fieldpath']} >= {33, 41, 61, 66}
    assert any(c[5] == 79 and c[3] == 8 for c in code['rootarrays'])
    assert any(c[5] == 33 and c[0] == E['OPCODES']['ILENA'] for c in code['lenarg'])
    assert {c[5] for c in code['listarg']} >= {112, 182}
    globals_ = dict((g[0], g[1]) for g in rows[0]['globals'])
    with (work / 'full.h').open('a') as stream:
        for name in ('nil', 'sys', 'bufio', 'str', 'saved', 'blanklex'):
            stream.write(f'static const uint lexer_mp_{name} = {globals_[name]};\n')
    return rows


def unit(rows):
    original = BASE['unit']()
    vm = original.split('\n#line 1 "native64-record-fields.c"')[0]
    vm = vm.replace('#define RuntimeBlocks 256', '#define RuntimeBlocks 1024', 1)
    vm += '''
#define LIMBO_NATIVE64_PREFIXES
#include "sys-native64.h"
#include "native64-sys-frame.h"
#include "native64-sys-sites.h"
#include "native64-sys-program.h"
#include "native64-sys-module.h"
'''
    for path, signatures in (
        ('emu/port/errstr.c', ('void\nkgerrstr(',)),
        ('libinterp/runt.c', ('int\nSys_setprogram64(', 'void\nSys_programcall64(', 'int\nSys_clearframes64(')),
        ('libinterp/string.c', ('String*\nstringdup(', 'void\ncvtup(')),
        ('libinterp/xec.c', ('static Inst*\ncasepc(', 'static void\nnewrecord(')),
    ):
        source_text = (ROOT / path).read_text()
        for sig in signatures:
            if sig not in vm:
                vm += '\n' + EXTRACT(source_text, sig) + '\n'
    start = vm.index(' optab[IMOVW]=countmovw;')
    handlers = vm[start:vm.index(' require(prepare64calls(', start)]
    xec = (ROOT / 'libinterp/xec.c').read_text()
    strings = (ROOT / 'libinterp/string.c').read_text()
    operations = {step[0] for row in rows for step in row['code']}
    for name, number in E['OPCODES'].items():
        if number not in operations:
            continue
        function = {'load': 'iload', 'goto': 'igoto', 'case': 'icase'}.get(name[1:].lower(), name[1:].lower())
        signature = 'OP(' + function + ')'
        if signature not in vm:
            vm += '\n' + EXTRACT(strings if signature + '\n{' in strings else xec, signature) + '\n'
        handlers += f' optab[{name}]=actual_{function};\n'
    return vm + '\n#line 1 "native64-field-arrays.c"\n' + (
        ROOT / 'utils/tests/native64-field-arrays.c').read_text().replace('FIELD_ARRAY_HANDLERS', handlers)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    if not __debug__:
        parser.error('source oracles require assertions')
    limbo = args.limbo.resolve()
    with tempfile.TemporaryDirectory(prefix='field-arrays-', dir=ROOT) as directory:
        work = Path(directory)
        CMD['generate'](limbo, work, 'Command', False, argv=True, local=True, pair=True, raises=True)
        rows = generate(limbo, work)
        (work / 'unit.c').write_text(unit(rows))
        arch = os.environ.get('NATIVE64_ARCH', 'amd64')
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        compiler += shlex.split(os.environ.get('NATIVE64_CFLAGS',
                                '-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        if arch in ('arm64', 'riscv64'):
            compiler += ['-DDIS64_TEST_' + arch.upper() + '=1', '-DDIS64_TEST_COMPILER="comp-' + arch + '.c"']
        compiler += ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1', '-DNAMESPACE_HEADER="provider.h"',
                     '-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid', '-I' + str(work)]
        compiler += ['-I' + str(ROOT / p) for p in ('Linux/' + arch + '/include', 'Linux/386/include',
                                                    'emu/port', 'include', 'libinterp')]
        binary = work / 'unit'
        command = compiler + [str(work / 'unit.c'), str(ROOT / 'libinterp/dec.c')] + \
            CMD['FILES']['releaseflags'](compiler) + ['-lm', '-o', str(binary)]
        runner = shlex.split(os.environ.get('NATIVE64_RUNNER', '')) + [str(binary)]
        subprocess.run(command, check=True)
        subprocess.run(runner, check=True, timeout=180)

        # Resolve the altered header through the same include chain as the VM.
        for name in ('native64-local-bind.h', 'native64-typed-module.h', 'native64-typed-multi.h',
                     'native64-typed-code.h', 'native64-raw-array-code.h'):
            (work / name).write_text((ROOT / 'include' / name).read_text())
        runtime = 'native64-raw-array-runtime.h'
        text = (ROOT / 'include' / runtime).read_text()
        assert text.count('  vmrun(&next);') == 1
        (work / runtime).write_text(text.replace('  vmrun(&next);', '', 1))
        subprocess.run(command, check=True)
        result = subprocess.run(runner, capture_output=True, text=True, timeout=180)
        assert result.returncode == 10 and 'within one scheduler quantum' in result.stderr, result.stderr
        assert 'runtime error:' not in result.stderr and 'Sanitizer' not in result.stderr
        print('PASS field array active control: separated index and consumer detected', flush=True)


if __name__ == '__main__':
    main()
