#include "typed-string-tables.h"
static void strcheck(int ok,int line){if(!ok){fprintf(stderr,"String table predicate line=%d\n",line);exit(10);}}
#undef require
#define require(c) strcheck(!!(c),__LINE__)

typedef struct Input Input;
struct Input {char *text;unsigned bytes;int expected,nilvalue;};
static const Input inputs[]={
    {"",0,1,0},{"",0,1,1},{"\0",1,6,0},{"\0x",2,5,0},
    {"a",1,2,0},{"a\0",2,2,0},{"b",1,2,0},{"c",1,2,0},{"cc",2,5,0},{"d",1,5,0},
    {"x\0z",3,7,0},{"x\0zZ",4,5,0},{"Привет",12,3,0},{"При",6,5,0},
    {"🚀",4,4,0},{"🚁",4,5,0},{"𐀀",4,5,0},
};

static void checkstrings(Module *m,uchar *data,int refs)
{
    Type *t=m->type[0];String *s;unsigned i;
    for(i=0;i<(unsigned)t->size/8;i++)if(i/8<(unsigned)t->np && (t->map[i/8]&(0x80>>(i%8)))){
        memmove(&s,data+8*i,8);
        if(s!=H)require(s!=nil && D2H(s)->t==&Tstring && D2H(s)->ref==refs);
    }
}

static Modlink *instance(Module *m,unsigned links)
{
    Modlink *ml;m->ref++;ml=mklinkmod(m,links);ml->MP=H2D(uchar*,heapz(m->type[0]));newmp(ml->MP,m->origmp,m->type[0]);return ml;
}

static void run(unsigned kind,const Input *input,int pjit,int cjit,int grow,int quantum,int runes)
{
    Native64ModuleOwner owner={0};Module *m,*pm;Modlink *callee,*idle,*link;Import request={0};
    Prog process;Frame *entry,*source;uchar *root;uchar map[]={0,0x80};void *result;String *value=H,*converted,*observed;
    unsigned i,offset=0;WORD arg=3,got;int done,before,reference=kind==1 || kind==2;
    uvlong guard=0xfedcba9876543210ULL,after;
    require(native64typedmodule(&spec,pjit,&owner));pm=owner.module;require(pm->native64casec==native64modulecasecstep);
    checkstrings(pm,pm->origmp,1);callee=instance(pm,1);idle=instance(pm,0);checkstrings(pm,pm->origmp,3);
    checkstrings(pm,callee->MP,3);checkstrings(pm,idle->MP,3);
    for(i=0;i<=kind;i++)offset+=functions[i].count;require(offset==legacy_first[kind]+1);
    request.name=(char*)functions[kind+1].name;memmove(&request.sig,&functions[kind+1].signature,4);
    require(linkm(pm,callee,0,&request)==0 && callee->links[0].u.pc==pm->prog+offset);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;
    m->name=strdup("StringCaseCaller");m->path=strdup("trusted/StringCaseCaller");m->ss=grow?0:4096;m->cstep=native64typedlea8step;
    m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->type!=nil);if(reference)map[1]|=0x40;
    m->type[0]=dtype(freeheap,8,nil,0);m->type[1]=dtype(freeheap,112,map,2);require(m->type[0] && m->type[1]);m->entryt=m->type[1];
    m->origmp=H2D(uchar*,heapz(m->type[0]));m->nprog=5;m->prog=mallocz(5*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;
    callinstruction(m->prog,IMFRAME,AFP,AFP,64,40);m->prog[0].add|=AXIMM;
    callargument(m->prog+1,reference?IMOVP:IMOVW,AFP,72,40,args[kind]);
    callargument(m->prog+2,ILEA,AFP,80,40,32);m->prog[2].reg=4;
    callinstruction(m->prog+3,IMCALL,AFP,AFP,40,64);m->prog[3].add|=AXIMM;callinstruction(m->prog+4,IEXIT,AXXX,AXXX,0,0);
    if(cjit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;
    entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(entry->t,entry);source=entry;R.s=&source;newstack(&process);free(entry);
    root=process.R.FP;memmove(root+64,&callee,8);
    if(reference){
        if(!input->nilvalue){
            value=c2string(input->text,input->bytes);
            if(runes && value->len>=0){
                converted=newrunes(value->len);for(i=0;i<(unsigned)value->len;i++)converted->Srune[i]=(uchar)value->Sascii[i];
                destroy(value);value=converted;
            }
        }
        memmove(root+72,&value,8);
    }else memmove(root+72,&arg,4);
    memmove(root+88,&guard,8);memset(root+80,0x5a,8);result=root+80;memmove(root+48,&result,8);
    before=raised;message[0]='x';process.quanta=quantum;
    for(i=0;i<10000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=10000 || message[0])fprintf(stderr,"String run kind=%u input=%u jit=%d/%d grow=%d q=%d rune=%d error=%s\n",kind,(unsigned)(input-inputs),pjit,cjit,grow,quantum,runes,message);
    require(i<10000 && raised==before+1 && !message[0] && !worker.nerr && R.FP==root);
    memmove(&got,result,4);require(got==(reference?input->expected:kind==0?arg+7:arg+9));
    memmove(&after,root+88,8);require(after==guard);for(i=4;i<8;i++)require(root[80+i]==0x5a);
    memmove(&observed,pm->origmp+current_offset,8);require(observed==H);
    memmove(&observed,idle->MP+current_offset,8);require(observed==H);
    memmove(&observed,callee->MP+current_offset,8);require(observed==(kind==2?value:H));
    if(reference && value!=H)require(D2H(value)->ref==(kind==2?2:1));
    destroystack(&R);require(m->ref==1);unload(m);checkstrings(pm,pm->origmp,2);
    destroy(idle);checkstrings(pm,pm->origmp,1);require(pm->ref==1 && native64typedmoduleclear(&owner));
    require(!live && !rootcount && Tstring.ref==1);
}

static void construction(void)
{
    Native64ModuleOwner owner={0};Module *m;Modlink *a,*b;REG saved=R;unsigned i;int jit,start,attempts;
    for(jit=0;jit<2;jit++){
        start=allocations;require(native64typedmodule(&spec,jit,&owner));attempts=allocations-start;m=owner.module;
        checkstrings(m,m->origmp,1);a=instance(m,0);b=instance(m,0);checkstrings(m,m->origmp,3);
        destroy(a);checkstrings(m,m->origmp,2);destroy(b);checkstrings(m,m->origmp,1);
        require(native64typedmoduleclear(&owner) && !live && !rootcount && Tstring.ref==1);
        for(i=1;i<=(unsigned)attempts;i++){
            failat=allocations+i;require(!native64typedmodule(&spec,jit,&owner));failat=0;
            require(!owner.module && !owner.rooted && !owner.self && !live && !rootcount && !memcmp(&saved,&R,sizeof(R)) && Tstring.ref==1);
        }
        printf("PASS String table ownership: jit=%d %d construction failure positions, two MP clones, exact root counts and full retirement\n",jit,attempts);
    }
}

static void aliases(void)
{
    Native64ModuleSpec s=spec;Native64ModuleFunction fn[5];Native64ModuleCase tables[2];Native64TypedStep code[256];Native64ModuleOwner owner={0};
    memmove(fn,functions,sizeof(fn));s.functions=fn;require(fn[2].count<nelem(code));
    memmove(code,fn[2].code,fn[2].count*sizeof(*code));fn[2].code=code;
    tables[0]=tables[1]=fn[2].cases[0];tables[1].pc++;code[tables[1].pc]=code[tables[0].pc];fn[2].cases=tables;fn[2].ncases=2;
    require(native64typedmodule(&s,0,&owner));checkstrings(owner.module,owner.module->origmp,1);
    require(native64typedmoduleclear(&owner) && !live && !rootcount && Tstring.ref==1);
    printf("PASS String table exact aliases: one owned String per MP slot, no overwritten/leaked owners\n");
}

static void borrowedmetadata(void)
{
    Native64ModuleSpec s=spec;Native64ModuleFunction fn[5];Native64ModuleCase table;Native64ModuleCaseRow rows[16];
    Native64ModuleString literals_copy[16];Native64ModuleOwner owner={0};uchar bound[]={'a'},text[64];String *value;
    memmove(fn,functions,sizeof(fn));s.functions=fn;table=fn[2].cases[0];fn[2].cases=&table;
    memmove(rows,table.rows,table.count*sizeof(*rows));table.rows=rows;require(rows[2].nstart==1 && rows[2].start[0]=='a');rows[2].start=bound;
    memmove(literals_copy,s.strings,s.nstrings*sizeof(*literals_copy));s.strings=literals_copy;
    require(literals_copy[0].bytes<sizeof(text));memmove(text,literals_copy[0].text,literals_copy[0].bytes);literals_copy[0].text=text;
    require(native64typedmodule(&s,0,&owner));bound[0]='Z';memset(text,0xa5,sizeof(text));
    memmove(&value,owner.module->origmp+table.offset+8+24*2,8);require(value->len==1 && value->Sascii[0]=='a');
    memmove(&value,owner.module->origmp+literals_copy[0].offset,8);require(value->len==-9 && value->Srune[8]==0x1f680);
    require(native64typedmoduleclear(&owner) && !live && !rootcount && Tstring.ref==1);
    printf("PASS String table borrowed metadata: label/global bytes poisoned after construction; owned Strings retain exact contents\n");
}

static int invoke(void *unused){USED(unused);return native64modulecasecstep();}
static void faults(void)
{
    Native64ModuleOwner owner={0};Module *m;Modlink *ml;Frame *frame;REG saved,prior=R;String *value;WORD original;unsigned n,pc=0;
    require(native64typedmodule(&spec,0,&owner));m=owner.module;ml=instance(m,0);
    for(n=0;n<=1;n++)pc+=functions[n].count;
    frame=mallocz(functions[2].bytes,1);require(frame!=nil);frame->t=m->type[3];initmem(frame->t,frame);
    for(n=0;n<4;n++){
        R=prior;R.M=ml;R.MP=ml->MP;R.FP=(uchar*)frame;R.PC=m->prog+pc+functions[2].cases[0].pc;
        value=c2string("outside",7);memmove((uchar*)frame+args[1],&value,8);
        memmove(&original,ml->MP+functions[2].cases[0].offset,4);
        if(n==0){String *null=nil;memmove((uchar*)frame+args[1],&null,8);}
        if(n==1){WORD too_many=4097;memmove(ml->MP+functions[2].cases[0].offset,&too_many,4);}
        if(n==2)value->len=(-2147483647-1);
        if(n==3){WORD invalid=m->nprog;unsigned at=functions[2].cases[0].offset+8+24*functions[2].cases[0].count;memmove(ml->MP+at,&invalid,4);}
        saved=R;require(!kprotect(invoke,nil) && !memcmp(&saved,&R,sizeof(R)) && !worker.nerr);
        require(!strcmp(message,n==0?exNilref:n==2?exHeap:exBounds));
        memmove(ml->MP+functions[2].cases[0].offset,&original,4);if(n==2)value->len=7;destroy(value);value=H;
        memmove((uchar*)frame+args[1],&value,8);
    }
    /* A bad unused default must not reject a valid matching row. Use the
     * actual low-bound owner to exercise String identity without retaining it. */
    R=prior;R.M=ml;R.MP=ml->MP;R.FP=(uchar*)frame;R.PC=m->prog+pc+functions[2].cases[0].pc;
    memmove(&value,ml->MP+functions[2].cases[0].offset+8+24*3,8);memmove((uchar*)frame+args[1],&value,8);
    require(D2H(value)->ref==2 && kprotect(invoke,nil));
    require(R.PC==m->prog+pc+functions[2].cases[0].rows[3].target && D2H(value)->ref==2);
    R=prior;free(frame);destroy(ml);require(native64typedmoduleclear(&owner) && !live && !rootcount && Tstring.ref==1);
    printf("PASS String table runtime refusals: four complete REG snapshots; identity match overrides invalid unused default\n");
}

static void refusals(void)
{
    Native64ModuleSpec s;Native64ModuleFunction fn[5];Native64ModuleCase table;Native64ModuleCaseRow rows[16];
    Native64ModuleOwner owner={0};Native64ModuleString text[16];uchar map[256];unsigned n,bit;int before;
    static const uchar invalid[]={0xf5,0x80,0x80,0x80};
    require(functions[2].ncases==1 && functions[2].cases[0].count<nelem(rows) && spec.nstrings<nelem(text) && spec.nstrings>0);
    for(n=0;n<16;n++){
        s=spec;memmove(fn,functions,sizeof(fn));s.functions=fn;table=fn[2].cases[0];fn[2].cases=&table;
        memmove(rows,table.rows,table.count*sizeof(*rows));table.rows=rows;memmove(map,s.datamap,s.mapbytes);s.datamap=map;
        memmove(text,s.strings,s.nstrings*sizeof(*text));s.strings=text;
        if(n==0){bit=(table.offset+8)/8;map[bit/8]^=0x80>>(bit%8);}
        if(n==1)s.string=nil;
        if(n==2)rows[0].start=nil;
        if(n==3){rows[table.count-1].start=invalid;rows[table.count-1].nstart=sizeof(invalid);}
        if(n==4)rows[1].target=fn[2].count;
        if(n==5)table.fallback=fn[2].count;
        if(n==6)rows[0].low=1;
        if(n==7){rows[0].stop=rows[1].start;rows[0].nstop=rows[1].nstart;}
        if(n==8)rows[0].nstart=4097;
        if(n==9)text[0].offset=table.offset+8;
        if(n==10){text[0].offset++;}
        if(n==11)text[0].text=H;
        if(n==12)text[0].bytes=4097;
        if(n==13)s.nstrings=65537;
        if(n==14)rows[0].nstop=1;
        if(n==15)table.offset=s.databytes;
        before=allocations;
        if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"String metadata refusal removed: case=%u\n",n);exit(10);}
        require(allocations==before && !owner.module && !owner.rooted && !live && !rootcount);
    }
    printf("PASS String table metadata: %u preallocation refusals, exact map/UTF-8/role/target/overlap checks\n",n);
}

int main(void)
{
    unsigned kind,v,p,c,g,q,r,cases=0;int quantum[]={1,3,1000};
    Tstring.free=freestring;
    optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;
    optab[IMOVW]=movw;optab[IMOVP]=movp;optab[IADDW]=addw;optab[ILENC]=lenc;optab[IEXIT]=iexit;
    refusals();construction();aliases();borrowedmetadata();faults();
    for(kind=0;kind<4;kind++)for(v=0;v<(kind==1 || kind==2?nelem(inputs):1);v++)
        for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<nelem(quantum);q++)for(r=0;r<2;r++){
            run(kind,inputs+v,p,c,g,quantum[q],r);cases++;
        }
    printf("PASS String table C bridge: %u module-call roots with C/JIT neighbours, ASCII/Rune/Unicode/NUL/nil/ranges/defaults, shifted targets and independent instances\n",cases);
    return 0;
}
