#include "lexer-native.h"
#include "full.h"
#include <stdarg.h>

static Module *services[nelem(full_tables)], *lexer_module;
static Type *fdtype;
static YYLEX *input;
static WORD inputchar;
static int loads, reads, prints, fds, promptmode;
static int indexes, uses, pending;
static const WORD marker=0x12345678;

static void
check(int ok, const char *message)
{
 if(!ok){
  fprintf(stderr,"lexer/field array predicate: %s\n",message);
  exit(10);
 }
}

static int
textis(String *s, const char *expected)
{
 return s!=nil&&s!=H&&s->len==(int)strlen(expected)&&!memcmp(s->Sascii,expected,s->len);
}

int
sprint(char *buffer, char *format, ...)
{
 va_list args;
 vlong word;
 check(!strcmp(format,"%lld"),"numeric string conversion format");
 va_start(args,format);
 word=va_arg(args,vlong);
 va_end(args);
 return snprintf(buffer,24,"%lld",(long long)word);
}

static void
ctl_fd(void *fp)
{
 F_Sys_fildes *f=fp;
 Sys_FD *value=H2D(Sys_FD*,heapz(fdtype));
 check(f->fd==2&&*f->ret==H,"stderr FD result");
 value->fd=2;
 *f->ret=value;
 fds++;
}

static void
ctl_getc(void *fp)
{
 void *value,*result;
 memmove(&value,(uchar*)fp+64,8);
 memmove(&result,(uchar*)fp+32,8);
 check(input!=nil&&input->f==value,"Bufio method gets the original self reference");
 memmove(result,&inputchar,4);
 reads++;
}

static void
ctl_in(void *fp)
{
 String *set;
 WORD rune,result;
 void *dest;
 memmove(&rune,(uchar*)fp+64,4);
 memmove(&set,(uchar*)fp+72,8);
 memmove(&dest,(uchar*)fp+32,8);
 check(set!=H&&set!=nil&&set->len>1&&set->Sascii[0]=='^',"original lexer set");
 result=(unsigned int)rune>127||!memchr(set->Sascii+1,rune,set->len-1);
 memmove(dest,&result,4);
}

int
Sys_fprint64(const Native64SysWriteRequest *request)
{
 const Native64SysFormatRequest *f=&request->format;
 Sys_FD *fd=request->fd;
 String *value;
 LONG n;
 WORD result=71;
 check(fd!=H&&fd!=nil&&D2H(fd)->t==fdtype&&fd->fd==2,"exact fprint FD");
 memmove(&value,f->values,8);
 if(promptmode){
  check(f->count==1&&textis(f->pattern,"%s")&&value==input->prompt,"exact prompt argument and format");
 }else{
  check(f->count==2&&textis(f->pattern,"%s %d")&&textis(value,"abc"),"exact mixed variadic format");
  memmove(&n,f->values+1,8);
  check(n==-123,"negative Word widened to a signed tail slot");
 }
 memmove(f->resolve(f->argument),&result,4);
 prints++;
 return 1;
}

int Sys_sprint64(const Native64SysFormatRequest *f){ USED(f);check(0,"unexpected sprint");return 0; }
int Sys_aprint64(const Native64SysFormatRequest *f){ USED(f);check(0,"unexpected aprint");return 0; }
int Sys_print64(const Native64SysWriteRequest *f){ USED(f);check(0,"unexpected print");return 0; }

static void (*
servicefn(const char *name))(void*)
{
 if(!strcmp(name,"Iobuf.getc"))return ctl_getc;
 if(!strcmp(name,"in"))return ctl_in;
 if(!strcmp(name,"fildes"))return ctl_fd;
 if(!strcmp(name,"fprint"))return Sys_programcall64;
 check(0,"known source import");
 return nil;
}

static Module*
service(const Native64ModuleTable *table)
{
 Module *m=mallocz(sizeof(*m),1);
 uint i;
 require(m!=nil);
 m->ref=1;
 m->origmp=H;
 m->ntype=table->count;
 m->name=strdup("lexer-service");
 m->path=strdup("lexer-service");
 m->type=mallocz(m->ntype*sizeof(Type*),1);
 m->ext=mallocz((m->ntype+1)*sizeof(Link),1);
 require(m->name&&m->path&&m->type&&m->ext);
 for(i=0;i<table->count;i++){
  const Native64ModuleImport *e=table->imports+i;
  m->type[i]=dtype(freeheap,e->bytes,(uchar*)e->map,e->mapbytes);
  m->ext[i].name=strdup(e->name);
  require(m->type[i]&&m->ext[i].name);
  memmove(&m->ext[i].sig,&e->signature,4);
  m->ext[i].frame=m->type[i];
  m->ext[i].u.runt=servicefn(e->name);
 }
 return m;
}

static Modlink*
fullload(Module *m, WORD table, char *path)
{
 Module *target;
 Modlink *link;
 const char *name;
 check(m==lexer_module&&table>=0&&table<(int)nelem(services),"exact source LOAD owner");
 name=full_tables[table].imports[0].name;
 check(!strcmp(path,!strcmp(name,"in")?"/dis/lib/string.dis":!strcmp(name,"Iobuf.getc")?"/dis/lib/bufio.dis":"$Sys"),
       "exact source LOAD path");
 loads++;
 target=services[table];
 target->ref++;
 link=linkmod(target,m->ldt[table],1);
 require(link!=H&&link!=nil);
 ptradd(D2H(link));
 return link;
}

static Module*
lexer_caller(void)
{
 Module *m=mallocz(sizeof(*m),1);
 Inst *p;
 uchar map[]={0,0xc0};
 require(m!=nil);
 m->ref=1;
 m->origmp=H;
 m->rt=DONTCOMPILE;
 m->ss=4096;
 m->cstep=native64typedlea8step;
 m->ntype=1;
 m->type=mallocz(sizeof(Type*),1);
 require(m->type!=nil);
 m->entryt=m->type[0]=dtype(freeheap,128,map,nelem(map));
 require(m->entryt!=nil);
 p=m->prog=mallocz(7*sizeof(*p),1);
 require(p!=nil);
 m->entry=p;
 m->nprog=7;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=72;p[0].d.ind=40;
 p[1].op=IMCALL;p[1].add=SRC(AFP)|AXIMM|DST(AFP);p[1].s.ind=40;p[1].d.ind=72;
 p[2]=p[0];p[2].reg=1;
 p[3].op=IMOVP;p[3].add=SRC(AFP)|DST(AFP|AIND);p[3].s.ind=64;p[3].d.i.f=40;p[3].d.i.s=64;
 p[4].op=ILEA;p[4].reg=4;p[4].add=SRC(AFP)|DST(AFP|AIND);p[4].s.ind=80;p[4].d.i.f=40;p[4].d.i.s=32;
 p[5]=p[1];p[5].reg=1;
 p[6].op=IEXIT;p[6].add=SRC(AXXX)|DST(AXXX);
 return m;
}

static void
field_index(void)
{
 int field=USRC((R.PC-1)->add)&AIND;
 actual_indx();
 if(field){
  check(!pending,"no outstanding field element address");
  indexes++;
  pending=1;
 }
}

static void
field_use(void)
{
 if(pending){
  uses++;
  pending=0;
 }
}
static void field_word(void){ field_use();actual_movw(); }
static void field_root(void){ field_use();actual_movp(); }
static void field_mapped(void){ field_use();actual_movmp(); }

static void
run_case(uint fn, void *arg, int jit, int quantum, WORD expected, const char *failure)
{
 Native64ModuleOwner owner={0};
 Import request[3]={{0}};
 const Native64ModuleFunction *specs[]={full_functions+full_fn_setup,full_functions+fn};
 Modlink *link;
 Module *outer;
 Prog *process;
 uchar *root;
 WORD result;
 uint i,steps;
 for(i=0;i<nelem(services);i++)services[i]=service(full_tables+i);
 fdtype=dtype(freeheap,sizeof(Sys_FD),nil,0);
 require(fdtype!=nil);
 check(native64sysmodule(&full_spec,full_variadics,nelem(full_variadics),jit,&owner),"checked lexer constructed");
 lexer_module=owner.module;
 for(i=0;i<nelem(specs);i++){
  request[i].name=(char*)specs[i]->name;
  memmove(&request[i].sig,&specs[i]->signature,4);
 }
 lexer_module->ref++;
 link=linkmod(lexer_module,request,1);
 require(link!=H&&link!=nil);
 outer=lexer_caller();
 if(jit)require(compile(outer,outer->nprog,nil));
 outer->ref++;
 starting=outer;
 started=nil;
 require(kprotect(start,nil));
 process=started;
 rootenvironment(process);
 root=process->R.FP;
 D2H(arg)->ref++;
 memmove(root+64,&arg,8);
 memmove(root+72,&link,8);
 memmove(root+80,&marker,4);
 memset(root+84,0x5a,44);
 loads=reads=prints=fds=indexes=uses=pending=0;
 worker.env->errstr[0]=0;
 process->quanta=quantum;
 for(steps=0;steps<10000;steps++){
  int more=kprotect(executeprocess,process);
  check(!pending&&indexes==uses,"field element address consumed within one scheduler quantum");
  if(!more)break;
 }
 memmove(&result,root+80,4);
 if(steps>=10000||strcmp(worker.env->errstr,failure)||result!=expected)
  fprintf(stderr,"%s jit=%d quantum=%d result=%d expected=%d error='%s' expected='%s' idx=%d\n",
          specs[1]->name,jit,quantum,result,expected,worker.env->errstr,failure,indexes);
 check(steps<10000&&!strcmp(worker.env->errstr,failure)&&result==expected,"exact lexer result or error");
 check(loads==nelem(services),"exact source service loads");
 for(i=84;i<128;i++)check(root[i]==0x5a,"caller guard");
 observed=process;
 progexit();
 emptyqueues();
 require(outer->ref==1);
 unload(outer);
 require(native64typedmoduleclear(&owner));
 lexer_module=nil;
 for(i=0;i<nelem(services);i++){
  check(services[i]->ref==1,"service link retired");
  unload(services[i]);
 }
 freetype(fdtype);
 check(D2H(arg)->ref==1&&!modules&&!worker.nerr,"caller/module ownership retired");
}

static YYLEX*
new_lexer(void)
{
 static uchar map[]={0xea,0x5a};
 Type *t=dtype(freeheap,sizeof(YYLEX),map,nelem(map));
 YYLEX *l;
 require(t!=nil);
 l=H2D(YYLEX*,heapz(t));
 freetype(t);
 l->linenum=11;
 return l;
}

static void
lexer_read(int jit, int quantum, int test)
{
 static const WORD chars[]={9731,-1,-1,31337,'\n',-1,-2,0,0};
 WORD words[]={123,31337},actual,want=chars[test];
 const char *failure="";
 Type *t;
 YYLEX *l=new_lexer();
 input=l;
 promptmode=1;
 if(test<3){
  l->s=c2string("a\342\230\203",4);
  l->strpos=test==0?1:2;
  l->eof=test==2;
 }else{
  uchar map[]={0xc0},elementmap[]={0x80};
  t=dtype(freeheap,sizeof(Bufio_Iobuf),map,1);
  require(t!=nil);
  l->f=H2D(Bufio_Iobuf*,heapz(t));
  freetype(t);
  t=dtype(freeheap,test==7?8:4,elementmap,test==7?1:0);
  require(t!=nil);
  l->cbuf=H2D(Array*,heaparray(t,2));
  freetype(t);
  if(test!=7)memmove(l->cbuf->data,words,sizeof(words));
  l->strpos=test==8?2:1;
  l->ncbuf=test==3||test>=7;
  l->lastnl=1;
  l->prompt=c2string(">>> ",4);
  inputchar=chars[test];
  if(test==6)want=-1;
  if(test>=7){want=marker;failure=exBounds;}
 }
 run_case(full_fn_read,l,jit,quantum,want,failure);
 check(reads==(test>=4&&test<=6)&&prints==reads&&fds==reads,"exact file read and prompt dispatch");
 check(indexes==(test>=3&&test<=6),"checked array dispatch count");
 check(l->linenum==11+(test==4)&&l->eof==(test==1||test==2||test==5||test==6),"original line and sticky EOF state");
 if(test>=3&&test<=6){
  memmove(&actual,l->cbuf->data+4,4);
  check(l->strpos==0&&l->ncbuf==0&&actual==want,"original ring wrap and selected element");
 }
 if(test==7)check(l->ncbuf==1,"element Type rejected before payload or buffer consumption");
 destroy(l);
 input=nil;
 check(!live&&!nlinkpins,"complete getc heap retirement");
}

static void
lexer_scan(int jit, int quantum)
{
 static const struct {
  const char *line,*word;
  WORD tok,parser,fd1,fd2;
 } cases[]={
  {"'a''b'","a'b",57348,6,0,0},
  {"alpha ","alpha",57348,6,0,0},
  {"|[2=1]",nil,'|',12,2,1},
  {">[3]",nil,57347,5,3,-1},
  {"&&",nil,57352,10,0,0},
  {"||",nil,57353,11,0,0},
  {"# comment\n",nil,'\n',14,0,0},
  {"'open",nil,57351,9,0,0},
 };
 uint i,token;
 for(i=0;i<nelem(cases);i++)for(token=0;token<2;token++){
  YYLEX *l=new_lexer();
  l->s=c2string((char*)cases[i].line,strlen(cases[i].line));
  run_case(token?full_fn_token:full_fn_scan,l,jit,quantum,token?cases[i].parser:cases[i].tok,"");
  if(cases[i].word)check(textis(l->lval.word,cases[i].word),"exact lexer word");
  if(cases[i].fd1)check(l->lval.redir!=H&&l->lval.redir->fd1==cases[i].fd1&&l->lval.redir->fd2==cases[i].fd2,"exact redirection tuple");
  if(cases[i].tok==57351)check(textis(l->err,"unterminated string literal")&&l->errline==11,"original error and line");
  check(!reads&&!prints&&!indexes,"string input stays in the string path");
  destroy(l);
  check(!live&&!nlinkpins,"complete lexer token heap retirement");
 }
}

static void
array_fields(int jit, int quantum)
{
 static uchar map[]={0xe8};
 Type *type=dtype(freeheap,sizeof(Store),map,nelem(map));
 Store *r;
 void *v;
 WORD n;
 require(type!=nil);
 r=H2D(Store*,heapz(type));
 freetype(type);
 r->i=1;
 r->n=3;
 r->text=c2string("abc",3);
 run_case(full_fn_arrays,r,jit,quantum,52,"");
 check(indexes==5&&r->words->len==2&&r->words->t->size==4&&r->texts->t->size==8&&
       r->items->t->size==16&&r->texts->t->map[0]==128&&r->items->t->map[0]==128,"Word, root and mixed field array geometry");
 memmove(&n,r->words->data,4);
 memmove(&v,r->texts->data+8,8);
 check(n==3&&v==r->text&&D2H(r->text)->ref==3,"exact slice, payload and root retention");
 r->n=-123;
 promptmode=0;
 run_case(full_fn_report,r,jit,quantum,71,"");
 check(fds==1&&prints==1&&reads==0,"mixed field arguments reached Sys");
 destroy(r);
 check(!live&&!nlinkpins,"complete generic field array retirement");
}

static void
metadata(void)
{
 Native64ModuleSpec spec=full_spec;
 Native64ModuleFunction functions[nelem(full_functions)];
 Native64TypedStep code[512];
 Native64ModuleOwner owner={0};
 uint i,j,k,total=0;
 int before=allocations;
 memmove(functions,full_functions,sizeof(functions));
 spec.functions=functions;
 check(native64sysmodulecheck(&spec,full_variadics,nelem(full_variadics)),"valid field metadata");
 for(i=0;i<nelem(functions);i++){
  const Native64ModuleFunction *base=full_functions+i;
  check(base->count<=nelem(code),"fixture function extent");
  functions[i].code=code;
  for(j=0;j<base->count;j++){
   const Native64TypedStep *a=base->code+j;
   if(native64rawfieldindex(a)){
    for(k=0;k<4;k++){
     memmove(code,base->code,base->count*sizeof(*code));
     if(k==0)code[j].s++;
     if(k==1)code[j].m^=8;
     if(k==2)code[j+1].returned=0;
     if(k==3)code[0]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,j+1,0};
     check(!native64sysmodule(&spec,full_variadics,nelem(full_variadics),0,&owner)&&
           !owner.module&&allocations==before,"malformed field pair rejected before allocation");
     total++;
    }
   }else if(a->returned==Native64TypedRecordArg||a->returned==Native64TypedRecordVararg){
    memmove(code,base->code,base->count*sizeof(*code));
    code[j].d=32;
    check(!native64sysmodule(&spec,full_variadics,nelem(full_variadics),0,&owner)&&
          !owner.module&&allocations==before,"malformed field argument rejected before allocation");
    total++;
   }
  }
  functions[i].code=base->code;
 }
 check(total>=30,"index and argument fixtures exercised");
 printf("PASS field array and argument metadata: %u operand, target and pending slot refusals\n",total);
}

int
main(void)
{
 int j,q,i,quanta[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;
 worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;
 worker.env=&worker.defenv;worker.type=Interp;
 Tarray.free=freearray;Tarray.size=sizeof(Array);Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));
 FIELD_ARRAY_HANDLERS
 optab[IINDX]=field_index;optab[IMOVW]=field_word;optab[IMOVP]=field_root;optab[IMOVMP]=field_mapped;
 check(limbo_native64_Lexer_layout(),"generated natural C geometry");
 metadata();
 for(j=0;j<2;j++)for(q=0;q<nelem(quanta);q++){
  for(i=0;i<9;i++)lexer_read(j,quanta[q],i);
  lexer_scan(j,quanta[q]);
  array_fields(j,quanta[q]);
 }
 puts("PASS array fields and unchanged Sh lexer: 162 interpreter/JIT cases with exact operands, tokens, file prompt, scheduler boundaries and ownership");
 return 0;
}
