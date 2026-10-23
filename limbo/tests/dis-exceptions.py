#!/usr/bin/env python3
"""Run production exception-table serialization at packed-count boundaries."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('DIS_EXCEPTION_SOURCE', root/'limbo/dis.c')).read_text()
start = source.index('void\ndisexc(Except *es)')
body = source[start:source.index('\n}', start)+2]
unit = r'''
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define nil NULL
enum {Tstring,Texception};
typedef struct Sym {char *name;} Sym;
typedef struct Type {int kind;} Type;
typedef struct Inst {int reach,pc;} Inst;
typedef struct Decl Decl;
typedef struct Node {Decl *decl;Type *ty;} Node;
struct Decl {Sym *sym;Node *init;int offset,id;};
typedef struct Label {Node *start;Inst *inst;} Label;
typedef struct Case {int nlab;Label *labs;Inst *iwild;} Case;
typedef struct Except {struct Except *next;Inst *p1,*p2;Case *c;Decl *d,*desc;int ne;} Except;
static jmp_buf escape;
static long output[70000];
static int count,strings,terminators,errors,bout,checks;
/* Record serialized fields independently of the packed-count calculation. */
static void discon(long value) {assert(count<70000);output[count++]=value;}
static int getpc(Inst *p) {return p->pc;}
static void Bprint(int stream,char *format,...) {(void)stream;(void)format;strings++;}
static void Bputc(int stream,int value) {(void)stream;assert(value==0);terminators++;}
static void fatal(char *message,...) {assert(message);errors++;longjmp(escape,1);}
IMPLEMENTATION
/* Require invalid counts to be rejected before any output, including the header. */
int main(void)
{
    int labels[]={-1,0,1,8191,8192,65535,65536,INT_MAX};
    int typed[]={-1,0,1,8191,8192,32768,65535,INT_MAX};
    Inst begin={1,10},end={0,20},target={0,30};
    Sym name={"exception"};Type type={Tstring};Decl decl={&name,nil,16,7};
    Node node={&decl,&type};Label *table;
    Case cases={0,nil,&target};Except entry={nil,&begin,&end,&cases,&decl,&decl,0};
    int a,b,i,valid,reachable;

    table=calloc(65536,sizeof(*table));assert(table);
    for(i=0;i<65536;i++){table[i].start=&node;table[i].inst=&target;}
    cases.labs=table;
    for(reachable=0;reachable<2;reachable++)for(a=0;a<8;a++)for(b=0;b<8;b++){
        cases.nlab=labels[a];entry.ne=typed[b];begin.reach=reachable;
        count=strings=terminators=errors=0;
        valid=labels[a]>=0 && labels[a]<=65535 && typed[b]>=0 && typed[b]<=labels[a] && typed[b]<=8191;
        if(setjmp(escape)==0)disexc(&entry);
        if(!reachable){assert(!errors && count==2 && output[0]==0 && output[1]==0);}
        else if(!valid){assert(errors==1 && count==0 && strings==0 && terminators==0);}
        else {
            assert(!errors && count==8+labels[a] && output[0]==1);
            assert(output[1]==16 && output[2]==10 && output[3]==20 && output[4]==7);
            assert(output[5]==(long)((uint32_t)labels[a]|((uint32_t)typed[b]<<16)));
            assert((output[5]&0xffff)==labels[a] && (output[5]>>16)==typed[b]);
            assert(output[count-2]==30 && output[count-1]==0);
            assert(strings==labels[a] && terminators==labels[a]);
        }
        checks++;
    }
    begin.reach=1;entry.c=nil;count=strings=terminators=errors=0;
    if(setjmp(escape)==0)disexc(&entry);
    assert(errors==1 && count==0);checks++;
    {
        Case badcase={-1,nil,nil};Except later=entry;
        cases.nlab=0;entry.ne=0;entry.c=&cases;entry.next=&later;
        later.next=nil;later.c=&badcase;
        count=strings=terminators=errors=0;
        if(setjmp(escape)==0)disexc(&entry);
        assert(errors==1 && count==0);checks++;
    }
    free(table);
    printf("Dis exception counts: %d cases\n",checks);
    return 0;
}
int __lsan_is_turned_off(void) {return 1;}
'''
with tempfile.TemporaryDirectory(prefix='dis-exceptions-') as directory:
    td = Path(directory)
    target = td/'test.c'
    target.write_text(unit.replace('IMPLEMENTATION', body))
    binary = td/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
