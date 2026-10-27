#!/usr/bin/env python3
"""Exercise real mk symbols under ASan/UBSan, including long bucket chains."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]  # Owning Inferno tree.
source = Path(os.environ.get('MK_SYMBOL_SOURCE', ROOT/'utils/mk/symtab.c')).read_text()
source = source.replace('#include\t"mk.h"', '')
prefix = r'''
#include "mk.h"
#include <stdio.h>
#include <stdarg.h>
static int live;
static char diagnostics[8192];
Biobuf bout;


/* Retain all checks with NDEBUG. */
static void
check(int condition)
{
    if(!condition)abort();
}


/* Count only symbol entries, whose names and values are borrowed. */
void*
Malloc(int size)
{
    void *p=malloc(size);
    check(p != nil);
    live++;
    return p;
}


/* Release real allocations so ASan observes any subsequent next-link read. */
static void
release(void *p)
{
    check(live > 0);
    live--;
    free(p);
}


/* Capture diagnostic text without replacing table traversal or bin selection. */
int
Bprint(Biobuf *unused, char *format, ...)
{
    va_list args;
    int n;
    size_t used=strlen(diagnostics);

    (void)unused;
    va_start(args,format);
    n=vsnprintf(diagnostics+used,sizeof(diagnostics)-used,format,args);
    va_end(args);
    return n;
}


/* Process-based leak discovery is unavailable; entry counts and real frees are checked. */
int
__lsan_is_turned_off(void)
{
    return 1;
}
#define free release // Track production symbol ownership only.
'''
suffix = r'''
#undef free


/* Long ASCII/UTF bytes exercise defined wrap and namespace isolation on LP64. */
static unsigned long
names(void)
{
    char (*keys)[256]=malloc(4096*256);
    Symtab *s;
    int i, space;
    unsigned long checks=0;

    check(keys != nil);
    for(i=0;i<4096;i++){
        snprintf(keys[i],256,"native-host-long-path-and-utf-\303\251-%d/environment/build/symbol",i);
        for(space=0;space<8;space++){
            void *value=(void*)((uintptr)0x100000000ULL+i*16+space+1);
            check(symlook(keys[i],space,nil)==nil);
            s=symlook(keys[i],space,value);
            check(s != nil && s->value==value);
            check(symlook(keys[i],space,(void*)1)==s && s->value==value);
            checks+=3;
        }
    }
    for(i=0;i<4096;i++)for(space=0;space<8;space++){
        void *value=(void*)((uintptr)0x100000000ULL+i*16+space+1);
        check(symlook(keys[i],space,nil)->value==value);
        symdel(keys[i],space);
        check(symlook(keys[i],space,nil)==nil);
        symdel(keys[i],space);
        checks+=2;
    }
    check(live==0);
    syminit();
    free(keys);
    return checks;
}


/* A one-entry deletion must never read next through the released entry. */
static void
deleteone(void)
{
    char key[]="short";
    check(symlook(key,0,(void*)1) != nil);
    symdel(key,0);
    check(live==0 && symlook(key,0,nil)==nil);
}


/* Valid linked bucket shapes may exceed the historical 1000-bin diagnostic array. */
static void
longbucket(void)
{
    int i;
    Symtab *s;

    for(i=0;i<3000;i++){
        s=Malloc(sizeof(*s));
        s->space=0;
        s->name="borrowed";
        s->value=(void*)1;
        s->next=hash[0];
        hash[0]=s;
    }
    symstat();
    check(strstr(diagnostics,"1 of length 999+") != nil);
    syminit();
    check(live==0);
}


/* Select independent parent probes without changing production implementation. */
int
main(void)
{
    char *mode=getenv("MK_SYMBOL_CASE");
    unsigned long total=0;

    if(mode==nil || !strcmp(mode,"hash"))total=names();
    if(mode==nil || !strcmp(mode,"delete"))deleteone();
    if(mode==nil || !strcmp(mode,"stat"))longbucket();
    printf("PASS mk symbols: %lu lookup/namespace/delete checks, singleton deletion and 3000-entry diagnostics\n",total);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='mk-symbols-') as directory:
    target=Path(directory)/'symbols.c'
    binary=Path(directory)/'symbols'
    target.write_text(prefix+source+suffix)
    command=shlex.split(os.environ.get('CC','cc'))
    command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command+=['-I'+str(ROOT/p) for p in ('Linux/386/include','include','utils/include','utils/mk')]
    subprocess.run(command+[str(target),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
