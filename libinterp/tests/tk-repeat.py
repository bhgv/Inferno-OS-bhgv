#!/usr/bin/env python3
"""Exercise production Tk autorepeat generations and reentrant one-shot callbacks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'libtk/utils.c').read_text()
globals_=source[source.index('static void *autorpt;'):source.index('/* blinking carets')]
functions=source[source.index('static int\nrptactive('):source.index('static int\nblinkactive(')]
harness=r'''
#include "lib9.h"
#include "draw.h"
#include "tk.h"
#include <assert.h>
#include <stdio.h>
static void *payload;
static int (*activefn)(void*);
static int (*checkfn)(void*,int);
static void (*firefn)(void*);
static int starts,wakes,calls,cancelled,rearm,fail_start;
static char worker;
void *rptproc(char *name,int tick,void *arg,int (*active)(void*),int (*ck)(void*,int),void (*fire)(void*)) {
    assert(strcmp(name,"autorepeat")==0 && tick==TkRptclick);starts++;
    if(fail_start)return nil;
    payload=arg;activefn=active;checkfn=ck;firefn=fire;return &worker;
}
void rptwakeup(void *arg,void *state) {assert(state==&worker);payload=arg;wakes++;}
GLOBALS
FUNCTIONS
static void callback(Tk *tk,void *note,int cancel) {
    assert(note==&worker);
    if(cancel){cancelled++;return;}
    calls++;
    if(rearm){rearm=0;tkrepeat(tk,callback,note,7,0);}
}
int main(void) {
    Tk a={0},b={0};
    void *stale;
    uintptr id;
    assert(sizeof(rptid)==sizeof(void*));
    tkrepeat(&a,callback,&worker,12,5);
    assert(starts==1 && activefn(payload) && !checkfn(payload,11) && checkfn(payload,12)==1);
    firefn(payload);assert(calls==1 && rptto==5);
    stale=payload;tkrepeat(&b,callback,&worker,3,4);
    assert(wakes==1 && cancelled==1 && !activefn(stale) && checkfn(stale,100)==-1);
    firefn(stale);assert(calls==1);
    tkcancelrepeat(&a);assert(activefn(payload));
    tkcancelrepeat(&b);assert(!activefn(payload));firefn(payload);assert(calls==1);
    tkrepeat(&a,callback,&worker,0,0);id=rptid;firefn(payload);
    assert(calls==2 && rptw==nil && rptid!=id && !activefn(payload));
    /* An old callback must not cancel a new one-shot armed from inside it. */
    tkrepeat(&a,callback,&worker,0,0);stale=payload;rearm=1;firefn(stale);
    assert(calls==3 && rptw==&a && activefn(payload) && !activefn(stale) && rptto==7);
    firefn(stale);assert(calls==3);
    firefn(payload);assert(calls==4 && rptw==nil && !activefn(payload));
    rptid=(uintptr)0x7ffffffeUL;
    tkrepeat(&a,callback,&worker,0,1);stale=payload;
    tkrepeat(&a,callback,&worker,0,1);
    assert(rptid==(uintptr)0x80000000UL && activefn(payload) && !activefn(stale));
    rptid=(uintptr)0xffffffffUL;tkrepeat(&a,callback,&worker,0,1);
    assert(activefn(payload));firefn(payload);assert(calls==5);
    rptid=~(uintptr)0;tkrepeat(&a,callback,&worker,0,1);
    assert(rptid==0 && activefn(payload));firefn(payload);assert(calls==6);
    tkcancelrepeat(&a);
    autorpt=nil;fail_start=1;tkrepeat(&a,callback,&worker,1,1);assert(autorpt==nil);
    fail_start=0;tkrepeat(&a,callback,&worker,1,1);assert(autorpt==&worker && activefn(payload));
    tkrepeat(nil,nil,nil,0,0);assert(rptw==nil && !activefn(payload));
    puts("PASS Tk autorepeat: pointer-width generations, overflow, stale events, cancellation, worker retry and reentrant one-shot callbacks");
    return 0;
}
'''.replace('GLOBALS',globals_).replace('FUNCTIONS',functions)
with tempfile.TemporaryDirectory(prefix='tk-repeat-') as directory:
    td=Path(directory);unit=td/'repeat.c';unit.write_text(harness)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g'))
    command+=['-Werror=pointer-to-int-cast','-Werror=int-to-pointer-cast']
    command+=['-I'+str(root/p) for p in ['Linux/386/include','include']]
    command+=[str(unit),'-o',str(td/'repeat')]
    subprocess.run(command,check=True)
    environment=dict(os.environ);environment.setdefault('UBSAN_OPTIONS','halt_on_error=1')
    subprocess.run([str(td/'repeat')],check=True,env=environment)
