#!/usr/bin/env python3
"""Run VM address/instruction diagnostics through the production lib9 formatter."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
rows = re.findall(r'"([^"\n]+)"\s*,\s*(I\w+)\s*,\s*TOKI([0-3])', (root/'libinterp/tab.h').read_text())
mapping = '\n'.join('    ['+op+']={"'+name+'",'+arity+'},' for name,op,arity in rows)
unit = r'''
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
int Dconv(Fmt*);
static struct {char *name;int arity;} expected[256]={
MAPPING
};
typedef struct TestAddr {uchar mode;Adr a;} TestAddr;
static int checks;
void rerrstr(char *p,uint n) {if(n)snprintf(p,n,"fixture");}
int __lsan_is_turned_off(void) {return 1;}
/* Preserve every byte outside the user-requested formatted destination. */
static void render(char *wanted,char *format,void *argument)
{
    struct {uchar before[16];char text[256];uchar after[16];} out;
    int capacity,i,n,length;
    length=strlen(wanted);
    for(capacity=1;capacity<=256;capacity=capacity==1?8:capacity==8?256:257){
        memset(&out,0xa5,sizeof(out));
        n=snprint(out.text,capacity,format,argument);
        assert(n==(length<capacity?length:capacity-1));
        assert(!memcmp(out.text,wanted,n) && out.text[n]==0);
        for(i=0;i<16;i++)assert(out.before[i]==0xa5 && out.after[i]==0xa5);
        for(i=capacity;i<256;i++)assert((uchar)out.text[i]==0xa5);
        checks++;
    }
}
/* Unknown byte opcodes must remain printable even when they exceed keywds. */
static void invalid(void)
{
    Inst i={0};char text[64];int op;
    for(op=255;op>=0;op--)if(!expected[op].name){
        i.op=op;snprintf(text,sizeof(text),"op %d",op);render(text,"%D",&i);
    }
    render("IZ","%D",nil);render("AZ","%a",nil);
}
/* Compare all address-mode tags and native-width signed operand boundaries. */
static void addresses(void)
{
    TestAddr a;char text[128];int mode,j;
    WORD minimum=(WORD)((UWORD)1<<(8*sizeof(WORD)-1));
    WORD values[]={0,1,-1,minimum,(WORD)((UWORD)minimum-1)};
    for(mode=0;mode<256;mode++)for(j=0;j<5;j++){
        memset(&a,0,sizeof(a));a.mode=mode;a.a.ind=values[j];
        switch(mode&AMASK){
        case AFP:snprintf(text,sizeof(text),"%ld(fp)",(long)a.a.ind);break;
        case AMP:snprintf(text,sizeof(text),"%ld(mp)",(long)a.a.ind);break;
        case AIMM:snprintf(text,sizeof(text),"$%ld",(long)a.a.imm);break;
        case AFP|AIND:snprintf(text,sizeof(text),"%u(%u(fp))",a.a.i.s,a.a.i.f);break;
        case AMP|AIND:snprintf(text,sizeof(text),"%u(%u(mp))",a.a.i.s,a.a.i.f);break;
        default:snprintf(text,sizeof(text),"A%d",mode&AMASK);break;
        }
        render(text,"%a",&a);
    }
}
/* Match each opcode's operand count and the signed 16-bit middle immediate. */
static void instructions(void)
{
    Inst i;char text[256],source[64],destination[64],middle[64];
    int op,mode,k;unsigned short registers[]={0,0x7fff,0x8000,0xffff};
    WORD minimum=(WORD)((UWORD)1<<(8*sizeof(WORD)-1));
    for(op=0;op<256;op++)if(expected[op].name)
    for(mode=AXNON;mode<=AXINM;mode+=AXIMM)for(k=0;k<4;k++){
        memset(&i,0,sizeof(i));i.op=op;i.add=SRC(AFP)|DST(AMP)|mode;i.reg=registers[k];
        i.s.ind=minimum+17;i.d.ind=(WORD)((UWORD)minimum-18);
        snprintf(source,sizeof(source),"%ld(fp)",(long)i.s.ind);
        snprintf(destination,sizeof(destination),"%ld(mp)",(long)i.d.ind);
        middle[0]=0;
        if(mode==AXIMM)snprintf(middle,sizeof(middle),", $%d",(int)(short)i.reg);
        if(mode==AXINF)snprintf(middle,sizeof(middle),", %u(fp)",i.reg);
        if(mode==AXINM)snprintf(middle,sizeof(middle),", %u(mp)",i.reg);
        if(expected[op].arity==0)snprintf(text,sizeof(text),"%s",expected[op].name);
        else if(expected[op].arity==1)snprintf(text,sizeof(text),"%s\t%s",expected[op].name,destination);
        else snprintf(text,sizeof(text),"%s\t%s%s, %s",expected[op].name,source,expected[op].arity==3?middle:"",destination);
        render(text,"%D",&i);
    }
}
int main(void)
{
    char *only=getenv("CONV_CASES");Inst i={0};char bootstrap[32];
    fmtinstall('D',Dconv);i.op=IRET;snprint(bootstrap,sizeof(bootstrap),"%D",&i);
    if(!only || !strcmp(only,"invalid"))invalid();
    if(!only || !strcmp(only,"addresses"))addresses();
    if(!only || !strcmp(only,"instructions"))instructions();
    printf("Dis diagnostics WORD%u: %d cases\n",(unsigned)(8*sizeof(WORD)),checks);return 0;
}
'''.replace('MAPPING',mapping)
names = ('rune','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','dorfmt')
with tempfile.TemporaryDirectory(prefix='dis-format-') as directory:
    td=Path(directory);target=td/'test.c';target.write_text(unit)
    for model in ('native','word32'):
        override=[]
        if model=='word32':
            folder=td/'word32';folder.mkdir();header=(root/'include/interp.h').read_text()
            header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
            header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            (folder/'interp.h').write_text(header);override=['-I'+str(folder)]
        cmd=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        includes=['-I'+str(root/p) for p in ('Linux/arm/include','include','lib9','libinterp')]
        source=Path(os.environ.get('DIS_FORMAT_SOURCE',root/'libinterp/conv.c'))
        binary=td/('test-'+model)
        library=[str(root/'lib9'/(n+'.c')) for n in names]
        if re.search(r'(?<!\w)sprint\(',source.read_text()): library.append(str(root/'lib9/sprint.c'))
        subprocess.run(cmd+override+includes+[str(target),str(source)]+library+['-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
