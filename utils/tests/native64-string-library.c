#include "full.h"
static void ck(int ok,const char*why){if(!ok){fprintf(stderr,"String library predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD n,char*p){USED(m);USED(n);USED(p);error(exModule);return H;}
static Module*active;static uint seen[21];
static int observe(void){VmFetch f;uint i,pc=0;if(vmfetch(&f)&&f.module==active)for(i=0;i<21;i++){if(f.pc>=active->prog+pc&&f.pc<active->prog+pc+full_functions[i].count){seen[i]=1;break;}pc+=full_functions[i].count;}return 0;}
static String*text(const char*s,int wide){String*a,*b;uint i;if(!s)return H;a=c2string((char*)s,strlen(s));if(wide&&a!=H&&a->len>=0){b=newrunes(a->len);for(i=0;i<(uint)a->len;i++)b->Srune[i]=(uchar)a->Sascii[i];destroy(a);a=b;}return a;}
static void equaltext(String*s,const char*want){char*value=string2c(s);if(strcmp(value,want?want:""))fprintf(stderr,"String actual=[%s] expected=[%s]\n",value,want?want:"");ck(!strcmp(value,want?want:""),"exact String value");}
static const char*words[]={"a","b c","d'e",""};static const char*appended[]={"a","b c","d'e","","tail"};
static List*list(int empty,int wide){List*l=H,*node;String*s;int i;if(!empty)for(i=3;i>=0;i--){s=text(words[i],wide);node=cons(8,&l);node->t=&Tptr;Tptr.ref++;memmove(node->data,&s,8);}return l;}
static void equallist(List*l,const char*const*want,uint n){uint i;String*s;for(i=0;i<n;i++){ck(l!=H,"exact List length");memmove(&s,l->data,8);equaltext(s,want[i]);l=l->tail;}ck(l==H,"exact List terminator");}
struct Case{const char*name,*a,*b;WORD number,word;LONG big;REAL real;const char*first,*second;const char*const*elements;uint count,empty;};
static const struct Case cases[]={
 {.name="splitl",.a="abc123",.b="0-9",.first="abc",.second="123"},
 {.name="splitl",.a="abc",.b="0-9",.first="abc",.second=""},
 {.name="splitl",.a=nil,.b="a-z",.first="",.second=""},
 {.name="splitr",.a="abc123",.b="a-z",.first="abc",.second="123"},
 {.name="splitr",.a="123",.b="a-z",.first="",.second="123"},
 {.name="drop",.a=" \tabc",.b=" \t",.first="abc"},
 {.name="drop",.a=nil,.b="a-z",.first=""},
 {.name="take",.a="abc123",.b="a-z",.first="abc"},
 {.name="take",.a="abc",.b="a-z",.first="abc"},
 {.name="in",.b="A-Z",.number='Z',.word=1},
 {.name="in",.b="^a-z",.number='q',.word=0},
 {.name="in",.b="-a-z",.number='-',.word=1},
 {.name="splitstrl",.a="ab--cd--ef",.b="--",.first="ab",.second="--cd--ef"},
 {.name="splitstrl",.a="abc",.b="",.first="",.second="abc"},
 {.name="splitstrr",.a="ab--cd--ef",.b="--",.first="ab--cd--",.second="ef"},
 {.name="splitstrr",.a="abc",.b="",.first="abc",.second=""},
 {.name="prefix",.a="ab",.b="abc",.word=1},
 {.name="prefix",.a="ac",.b="abc",.word=0},
 {.name="prefix",.a=nil,.b="abc",.word=1},
 {.name="tolower",.a="AB\316\273Z",.first="ab\316\273z"},
 {.name="tolower",.a=nil,.first=""},
 {.name="toupper",.a="ab\316\273z",.first="AB\316\273Z"},
 {.name="toupper",.a=nil,.first=""},
 {.name="toint",.a="-2147483648tail",.number=10,.word=(-2147483647-1),.second="tail"},
 {.name="toint",.a="z!",.number=36,.word=35,.second="!"},
 {.name="toint",.a="16rFF!",.number=0,.word=255,.second="!"},
 {.name="toint",.a="123",.number=1,.word=0,.second="123"},
 {.name="toint",.a=nil,.number=10,.word=0,.second=""},
 {.name="tobig",.a="9223372036854775807!",.number=10,.big=9223372036854775807LL,.second="!"},
 {.name="tobig",.a="-9223372036854775808!",.number=10,.big=(-9223372036854775807LL-1),.second="!"},
 {.name="tobig",.a="16rFF!",.number=0,.big=255,.second="!"},
 {.name="tobig",.a="123",.number=37,.big=0,.second="123"},
 {.name="toreal",.a="1.25e2tail",.number=10,.real=125,.second="tail"},
 {.name="toreal",.a="16rA.8!",.number=0,.real=10.5,.second="!"},
 {.name="toreal",.a="infinityend",.number=10,.real=INFINITY,.second="end"},
 {.name="toreal",.a="nanrest",.number=10,.real=NAN,.second="rest"},
 {.name="toreal",.a=nil,.number=10,.real=0,.second=""},
 {.name="append",.a="tail",.elements=appended,.count=5},
 {.name="append",.a="tail",.empty=1,.elements=appended+4,.count=1},
 {.name="quoted",.first="a 'b c' 'd''e' ''"},
 {.name="quoted",.empty=1,.first=""},
 {.name="quotedc",.b="a",.first="'a' 'b c' 'd''e' ''"},
 {.name="unquoted",.a="a 'b c' 'd''e' ''",.elements=words,.count=4},
 {.name="unquoted",.a=nil,.count=0},
};
static Module*rootmodule(const struct LibraryFunction*f,int grow){Module*m;Inst*p;uchar map[3]={0,8,0};uint i,pc=0;
 for(i=0;i<f->argc;i++)if(f->kinds[i]!=9)map[1]|=128>>i;if(f->contract==48)map[2]=0xc0;else if(f->contract==16)map[2]=0x40;else if(f->contract==24)map[2]=0x80;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("String-caller");m->path=strdup("String-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,192,map,3);m->prog=p=mallocz(7*sizeof(Inst),1);m->entry=p;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=96;p[pc++].d.ind=40;
 for(i=0;i<f->argc;i++){p[pc].op=f->kinds[i]==9?IMOVW:IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64+8*i;p[pc].d.i.f=40;p[pc++].d.i.s=64+8*i;}
 p[pc].op=ILEA;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=128;p[pc].reg=f->contract;p[pc].d.i.f=40;p[pc++].d.i.s=32;
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=96;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;
}
static void run(uint ci,int jit,int quantum,int wide){const struct Case*c=cases+ci;const struct LibraryFunction*f=nil;Native64ModuleOwner owner={0};Module*m;Import imports[2]={{0}};Modlink*ml;String*a,*b,*out;List*l,*xs;Prog*p;uchar*root;uint i,steps;WORD word;LONG big;REAL real;int done;
 for(i=0;i<nelem(library_functions);i++)if(!strcmp(full_functions[library_functions[i].fn].name,c->name)){f=library_functions+i;break;}ck(f!=nil,"public String entry found");ck(native64typedmodule(&full_spec,jit,&owner),"complete String constructor");active=owner.module;active->native64sysstep=observe;
 a=text(c->a,wide);b=text(c->b,wide);xs=list(c->empty,wide);if(a!=H)ptradd(D2H(a));if(b!=H)ptradd(D2H(b));if(xs!=H)ptradd(D2H(xs));imports[0].name=(char*)full_functions[f->fn].name;imports[0].sig=full_functions[f->fn].signature;owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml&&ml!=H);m=rootmodule(f,wide);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+96,&ml,8);
 for(i=0;i<f->argc;i++){if(f->kinds[i]==9){memmove(root+64+8*i,&c->number,4);memset(root+68+8*i,0x69,4);}else{void*v=f->kinds[i]==10?(void*)xs:i?(void*)b:(void*)a;if(v!=H)D2H(v)->ref++;memmove(root+64+8*i,&v,8);}}
 memset(root+160,0x5a,32);if(f->result==109)memset(root+132,0x6a,4);p->quanta=quantum;
 for(steps=0;steps<100000;steps++){done=kprotect(executeprocess,p);if(!done)break;}if(steps>=100000||worker.env->errstr[0])fprintf(stderr,"String fn=%s case=%u jit=%d q=%d wide=%d error=%s\n",c->name,ci,jit,quantum,wide,worker.env->errstr);ck(steps<100000&&!worker.env->errstr[0],"complete String execution");
 for(i=160;i<192;i++)ck(root[i]==0x5a,"String result caller guard");if(f->result==109)for(i=132;i<136;i++)ck(root[i]==0x6a,"int result neighbor guard");for(i=0;i<f->argc;i++)if(f->kinds[i]==9){uint k;for(k=68+8*i;k<72+8*i;k++)ck(root[k]==0x69,"Word32 argument neighbor guard");}
 if(f->result==13){memmove(&out,root+128,8);equaltext(out,c->first);}else if(f->result==9){memmove(&word,root+128,4);ck(word==c->word,"exact Word32 result");}else if(f->result==100){memmove(&out,root+128,8);equaltext(out,c->first);memmove(&out,root+136,8);equaltext(out,c->second);}else if(f->result==10){memmove(&l,root+128,8);equallist(l,c->elements,c->count);}else{memmove(&out,root+136,8);equaltext(out,c->second);if(f->result==109){memmove(&word,root+128,4);ck(word==c->word,"exact parsed Word32");}else if(f->result==104){memmove(&big,root+128,8);ck(big==c->big,"exact parsed Big");}else{memmove(&real,root+128,8);ck(isnan(c->real)?isnan(real):real==c->real,"exact parsed Real");}}
 equaltext(a,c->a);equaltext(b,c->b);equallist(xs,words,c->empty?0:4);observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);ck(native64typedmoduleclear(&owner),"String module retired");void*objects[]={a,b,xs};for(i=0;i<3;i++)if(objects[i]!=H){ck(D2H(objects[i])->ref==1,"String input owners retired");ptrdel(D2H(objects[i]));destroy(objects[i]);}ck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tstring.ref==1&&Tlist.ref==1&&Tptr.ref==1&&Tmodlink.ref==1,"complete String retirement");memset(nativeobservations,0,sizeof(nativeobservations));}
static void metadata(void){Native64ModuleSpec spec;Native64ModuleFunction f[nelem(full_functions)];Native64TypedStep code[1024];Native64ModuleOwner o={0};REG saved=R;uint fn,pc,k,total=0;int before;
 for(fn=0;fn<nelem(full_functions);fn++)for(pc=0;pc<full_functions[fn].count;pc++){uint tag=full_functions[fn].code[pc].returned;if(tag!=137&&tag!=152&&!(tag>=145&&tag<=151)&&full_functions[fn].code[pc].op!=IINSC)continue;
 for(k=0;k<5;k++){spec=full_spec;memmove(f,full_functions,sizeof(f));ck(f[fn].count<=nelem(code),"metadata fixture capacity");memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;spec.functions=f;
 if(k==0){code[pc].s=0x1000000;code[pc].mode=(code[pc].mode&~SRC(7))|SRC(AFP);}else if(k==1)code[pc].d=0x1000000;else if(k==2)code[pc].mode=255;else if(k==3)code[pc].returned=255;else code[pc].m=0x10000000;
 before=allocations;{int accepted=native64typedmodule(&spec,0,&o);if(accepted)fprintf(stderr,"String refusal fn=%u pc=%u tag=%u mutation=%u\n",fn,pc,tag,k);ck(!accepted,"malformed String metadata refused");}ck(allocations==before&&!live&&!o.module&&!memcmp(&R,&saved,sizeof(R)),"String refusal before allocation");total++;}}
 ck(total>0,"String metadata controls exercised");printf("PASS String metadata: %u refusals before allocation\n",total);}
static void construction(void){Native64ModuleOwner o={0};REG saved=R;int jit,b,n,i;for(jit=0;jit<2;jit++){b=allocations;ck(native64typedmodule(&full_spec,jit,&o),"String constructor baseline");n=allocations-b;ck(native64typedmoduleclear(&o),"String baseline clear");for(i=1;i<=n;i++){failat=allocations+i;ck(!native64typedmodule(&full_spec,jit,&o),"String constructor failure");failat=0;ck(!live&&!o.module&&!memcmp(&R,&saved,sizeof(R)),"String constructor rollback");}printf("PASS String constructor jit=%d: %d failures\n",jit,n);}}
int main(void){uint ci,total=0,i;int jit,q,wide,quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freestring;require(dis64selectlaunch("dis64-calls"));
 LIBRARY_HANDLERS
 optab[ILENC]=actual_lenc;optab[IMOVP]=actual_movp;optab[IADDC]=actual_addc;optab[IINDC]=actual_indc;optab[IINSC]=actual_insc;optab[ISLICEC]=actual_slicec;optab[ICVTCF]=actual_cvtcf;
 metadata();construction();
 for(ci=0;ci<nelem(cases);ci++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(wide=0;wide<2;wide++){run(ci,jit,quanta[q],wide);total++;}for(i=0;i<21;i++){if(!seen[i])fprintf(stderr,"unseen String body: %s\n",full_functions[i].name);ck(seen[i],"all 21 real String bodies executed");}printf("PASS complete String library: %u C/JIT scenarios, every 21 original bodies executed, all 17 public entries, ASCII/Rune/nil, pairs and numeric tuples, boundaries and complete retirement\n",total);return 0;}
