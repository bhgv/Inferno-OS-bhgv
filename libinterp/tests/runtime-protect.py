#!/usr/bin/env python3
"""Exercise production runtime protection with real emulator Proc error frames.

The native-kernel body is also exercised through the emulator error-stack
adapter; this does not replace a complete native-kernel integration test.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
unit = r'''
#include "dat.h"
#include "fns.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
static Proc worker;
static char *last_error;
Proc *getup(void) { return &worker; }
void nexterror(void) {
    assert(worker.nerr > 0);
    siglongjmp(worker.estack[--worker.nerr], 1);
}
void error(char *text) { last_error=text; nexterror(); }
int __lsan_is_turned_off(void) { return 1; }
GUARD
static int result(void *arg) {
    assert(worker.nerr == 2);
    return *(int*)arg;
}
static int fail(void *arg) {
    (void)arg;
    assert(worker.nerr >= 2);
    error("injected");
    return 1;
}
static int nested(void *arg) {
    assert(worker.nerr == 2);
    assert(!kprotect(fail, arg));
    assert(worker.nerr == 2);
    assert(strcmp(last_error,"injected")==0);
    krethrow();
    return 1;
}
int main(void) {
    int i, value;
    for(i=0; i<100; i++) {
        if(waserror()) {
            assert(worker.nerr == 0);
            continue;
        }
        for(value=-1; value<=1; value++) {
            assert(kprotect(result, &value) == value);
            assert(worker.nerr == 1);
        }
        assert(!kprotect(fail, nil));
        assert(worker.nerr == 1);
        assert(!kprotect(nested, nil));
        assert(worker.nerr == 1);
        error("outer frame must still work");
        assert(0);
    }
    assert(worker.nerr == 0);
    puts("PASS runtime protection: returns, nested VM errors/rethrow, outer error frame and 100 retries");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='runtime-protect-') as directory:
    td = Path(directory)
    for runtime in ['emu/port/dis.c', 'os/port/dis.c']:
        source = (root/runtime).read_text()
        guard = source[source.index('int\nkprotect('):]
        path = td/'protect.c'
        path.write_text(unit.replace('GUARD', guard))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O2 -g'))
        command += ['-I'+str(root/p) for p in ['Linux/arm/include', 'emu/port', 'include']]
        command += [str(path), '-o', str(td/'protect')]
        subprocess.run(command, check=True)
        subprocess.run([str(td/'protect')], check=True)
