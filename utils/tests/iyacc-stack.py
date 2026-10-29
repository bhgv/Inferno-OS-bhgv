#!/usr/bin/env python3
"""Execute generated parsers at stack limits, through empty rules and debug recovery."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Generator and matching installed skeleton.
PROLOGUE = r'''%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static const char *input;
static int result, errors, overflow;
int yylex(void);
int yyparse(void);
extern int yynerrs, yyerrflag;
void yyerror(char*);
int fprint(int, char*, ...);
int sprint(char*, char*, ...);
%}
%%
input: expression { result = $1; }
     | 'z' empty { result = $2; }
RECOVERY
;
empty: ;
expression: '1' { $$ = 1; }
          | '(' expression ')' { $$ = $2+1; }
          | expression '+' '1' { $$ = $1+1; }
;
%%


/* Keep address/stack checks while avoiding unavailable process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Return one fixture token without depending on the production Limbo lexer. */
int
yylex(void)
{
    yylval = 5;
    return *input ? *input++ : 0;
}


/* Distinguish grammar rejection from the explicit stack-capacity diagnostic. */
void
yyerror(char *message)
{
    errors++;
    if(strcmp(message, "yacc stack overflow") == 0)
        overflow++;
}


/* Evaluate every debug argument while keeping verbose parser output bounded. */
int
fprint(int fd, char *format, ...)
{
    (void)fd;
    (void)format;
    return 0;
}


/* Debug token/state names need only their bounded integer spelling. */
int
sprint(char *buffer, char *format, ...)
{
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, 16, format, arguments);
    va_end(arguments);
    return length;
}


/* Confirm both result semantics and restoration of the parser's global image. */
int
main(int argc, char **argv)
{
    int code, restored;

    if(argc != 2)
        return 71;
    input = argv[1];
    result = -1;
    yylval = 11;
    yyval = 13;
    yynerrs = 17;
    yyerrflag = 19;
    code = yyparse();
    restored = yylval == 11 && yyval == 13 && yynerrs == 17 && yyerrflag == 19;
    printf("%d %d %d %d %d\n", code, result, errors, overflow, restored);
    return 0;
}
'''


# Run complete generated parsers; keep sanitizer diagnostics distinguishable from syntax errors.
def main():
    command = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS',
        '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cases = 0
    with tempfile.TemporaryDirectory(prefix='iyacc-stack-') as temporary:
        work = Path(temporary)
        generator = os.environ.get('IYACC')
        if not generator:
            tools = work/'tools'
            environment = dict(os.environ, INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_BINDIR=str(tools), MKBOOTSTRAP_LIMBO='0')
            subprocess.run(['sh', str(ROOT/'makemk.sh')], env=environment,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
            generator = str(tools/'iyacc')
        template = Path(os.environ.get('IYACC_TEMPLATE', ROOT/'utils/iyacc/yaccpar')).read_bytes()
        if not os.environ.get('IYACC_TEMPLATE') and template != (ROOT/'utils/lib/yaccpar').read_bytes():
            raise RuntimeError('installed and source parser skeletons differ')
        destination = work/'template-root/utils/lib'
        destination.mkdir(parents=True)
        (destination/'yaccpar').write_bytes(template)
        for recovery in (False, True):
            for debug in (0, 4):
                directory = work/(str(int(recovery))+'-'+str(debug))
                directory.mkdir()
                grammar = PROLOGUE.replace('RECOVERY', "| error ';' { result = 777; yyerrok; }" if recovery else '')
                (directory/'probe.y').write_text(grammar)
                subprocess.run([generator, '-D', str(debug), 'probe.y'], cwd=directory,
                               env=dict(os.environ, ROOT=str(work/'template-root')), check=True)
                for capacity in (1, 2, 3, 4, 8, 16, 32):
                    binary = directory/('parser-'+str(capacity))
                    subprocess.run(command+flags+['-DYYMAXDEPTH='+str(capacity),
                        str(directory/'y.tab.c'), '-o', str(binary)], cwd=directory, check=True)
                    inputs = [('('*depth+'1'+')'*depth, 1+depth)
                              for depth in range(capacity+3)]
                    inputs += [('z', 0), ('1+1+1', 3), ('?', None), ('?;', None),
                               ('((?;', None), ('((1', None), ('1+', None), ('', None)]
                    saw_valid = False
                    for text, expected in inputs:
                        result = subprocess.run([str(binary), text], text=True, capture_output=True)
                        if result.returncode:
                            raise RuntimeError(str((capacity, recovery, debug, text))+'\n'+result.stderr)
                        code, value, errors, overflow, restored = map(int, result.stdout.split())
                        if not restored:
                            raise RuntimeError('parser globals were not restored')
                        if expected is not None:
                            if code == 0:
                                if value != expected or errors or overflow:
                                    raise RuntimeError(('semantic mismatch', text, result.stdout))
                                saw_valid = True
                            elif code != 1 or overflow != 1 or errors != 1:
                                raise RuntimeError(('unexplained valid-input rejection', text, result.stdout))
                        elif errors == 0 or code not in (0, 1):
                            raise RuntimeError(('missing syntax/overflow error', text, result.stdout))
                        cases += 1
                    if capacity >= 8 and not saw_valid:
                        raise RuntimeError('parser rejected all ordinary valid inputs')
    print('PASS generated iyacc stacks: '+str(cases)+' boundary/empty/debug/recovery/global-restoration cases')


# Treat a sanitizer failure as a failed parser execution, never as expected syntax rejection.
if __name__ == '__main__':
    main()
