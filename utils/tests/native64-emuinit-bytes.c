#include "bytes.h"
static void bytecheck(int ok,char*why){if(!ok){fprintf(stderr,"byte-array predicate: %s\n",why);exit(10);}}
static Modlink*byteinstance(Module*m,int fn){Import r[2]={{0}};Modlink*l;r[0].name=(char*)bytes_functions[fn].name;memmove(&r[0].sig,&bytes_functions[fn].signature,4);m->ref++;l=linkmod(m,r,1);require(l!=H);return l;}
static Module*bytelaunch(int jit,int grow,int fn){Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,0xe0};int at=0;
 require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);require(m->type[0]!=nil);m->entryt=m->type[0];p=m->prog=mallocz(6*sizeof(*p),1);require(p!=nil);m->entry=p;
 p[at].op=IMFRAME;p[at].add=SRC(AFP)|AXIMM|DST(AFP);p[at].s.ind=72;p[at++].d.ind=40;
 if(fn){p[at].op=IMOVP;p[at].add=SRC(AFP)|DST(AFP|AIND);p[at].s.ind=64;p[at].d.i.f=40;p[at++].d.i.s=64;}
 p[at].op=IMOVW;p[at].add=SRC(AFP)|DST(AFP|AIND);p[at].s.ind=56;p[at].d.i.f=40;p[at++].d.i.s=fn?72:64;
 p[at].op=ILEA;p[at].reg=24;p[at].add=SRC(AFP)|DST(AFP|AIND);p[at].s.ind=80;p[at].d.i.f=40;p[at++].d.i.s=32;
 p[at].op=IMCALL;p[at].add=SRC(AFP)|AXIMM|DST(AFP);p[at].s.ind=40;p[at++].d.ind=72;p[at].op=IEXIT;p[at++].add=SRC(AXXX)|DST(AXXX);m->nprog=at;if(jit)require(compile(m,at,nil));return m;
}
static int byterun(int jit,int grow,int q,int fn,int n,int failure,int unicode){Native64ModuleOwner owner={0};Modlink*l;Module*outer;Prog*process;Array*a=H;String*s;uchar*root;uint done,i;int before,steps,used;WORD count=n;const char*error;
 require(native64typedmodule(&bytes_spec,jit,&owner));bytecheck(owner.module->ntype==4 && owner.module->type[3]->size==1 && owner.module->type[3]->np==0,"separate owned byte descriptor");{uint k;for(k=0;k<owner.module->nprog;k++)if(owner.module->prog[k].op==INEWAZ)bytecheck(owner.module->prog[k].reg==owner.module->ntype-1,"resolved byte descriptor");}l=byteinstance(owner.module,fn);
 if(fn){a=H2D(Array*,heaparray(&Tbyte,8));memmove(a->data,unicode?"мир\0x":"a\0bcdefg",8);}
 outer=bytelaunch(jit,grow,fn);starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;memmove(root+64,&a,8);memmove(root+72,&l,8);memmove(root+56,&count,4);memset(root+88,0xa5,40);process->quanta=q;
 before=allocations;if(failure)failat=allocations+failure;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}failat=0;error=worker.env->errstr;
 bytecheck(steps<10000,"bounded execution");if(!error[0]){memmove(&s,root+80,8);bytecheck(s!=H,"owned String result");
 if(fn && unicode){static const Rune runes[]={0x43c,0x438,0x440,0,'x'};int expected=n==0?0:n==4?2:5;bytecheck((s->len<0?-s->len:s->len)==expected,"Unicode slice length");for(i=0;i<(uint)expected;i++)bytecheck((s->len<0?s->Srune[i]:s->Sascii[i])==runes[i],"Unicode slice rune");}
 else{bytecheck(s->len==n,"complete result length");for(i=0;i<(uint)n;i++)bytecheck(s->Sascii[i]==(fn?a->data[i]:0),"slice bytes or zero initialization");}}

 else bytecheck(!strcmp(error,exHeap)||!strcmp(error,exNomem),"allocation error family");
 used=allocations-before;if(fn)bytecheck(a->len==8 && (error[0] || D2H(a)->ref==1) && !memcmp(a->data,unicode?"мир\0x":"a\0bcdefg",8),"source array unchanged");
 for(i=88;i<128;i++)bytecheck(root[i]==0xa5,"caller guards");observed=process;progexit();emptyqueues();require(owner.module->ref==1);require(native64typedmoduleclear(&owner));bytecheck(!live&&!nlinkpins&&!modules&&Tarray.ref==1&&Tbyte.ref==1&&Tstring.ref==1,"complete array/type/root retirement");return used;
}
static void bytepreflight(void){Native64ModuleSpec spec=bytes_spec;Native64ModuleFunction functions[2];Native64TypedStep code[64];Native64ModuleOwner owner={0};uchar map[64];REG saved=R;int i,before;uint bit;
 for(i=0;i<8;i++){memmove(functions,bytes_functions,sizeof(functions));spec.functions=functions;memmove(code,functions[0].code,functions[0].count*sizeof(*code));functions[0].code=code;memmove(map,functions[0].map,functions[0].mapbytes);functions[0].map=map;
 if(i==0){bit=code[0].d/8;map[bit/8]&=~(0x80>>(bit%8));}if(i==1){bit=code[0].s/8;map[bit/8]|=0x80>>(bit%8);}
 if(i==2)code[0].m=1;if(i==3)code[0].d++;if(i==4)code[0].returned=24;if(i==5)code[0].s=functions[0].bytes;if(i==6)code[0].mode=(code[0].mode&~ARM)|AXINF;if(i==7)code[1].returned=8;
 before=allocations;if(native64typedmodule(&spec,0,&owner)){fprintf(stderr,"byte metadata refusal removed: case=%d\n",i);exit(10);}bytecheck(allocations==before&&!live&&!owner.module&&!memcmp(&saved,&R,sizeof(R)),"metadata refusal before allocation");}
 puts("PASS byte arrays: eight root/scalar/token/encoding refusals before allocation");
}
static void byteconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int j,i,n,before;
 for(j=0;j<2;j++){before=allocations;require(native64typedmodule(&bytes_spec,j,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
 for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&bytes_spec,j,&owner));failat=0;bytecheck(!owner.module&&!owner.rooted&&!live&&!nlinkpins&&!memcmp(&saved,&R,sizeof(R)),"primitive Type construction rollback");}
 printf("PASS byte constructor: jit=%d %d allocation failures including primitive Type\n",j,n);}
}
int main(int argc,char**argv){int j,g,q,fn,n,f,u,used,total=0;int quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;Tarray.size=sizeof(Array);require(dis64selectlaunch("dis64-calls"));
 bytepreflight();byteconstruction();if(argc==2)return 0;USED(argv);
 BYTES_HANDLERS
 optab[IMOVP]=actual_movp;optab[INEWAZ]=actual_newaz;optab[ISLICEA]=actual_slicea;optab[ICVTAC]=actual_cvtac;
 for(j=0;j<2;j++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(fn=0;fn<2;fn++)for(n=0;n<9;n+=4)for(u=0;u<2;u++){
 used=byterun(j,g,quanta[q],fn,n,0,u);total++;for(f=1;f<=used;f++){byterun(j,g,quanta[q],fn,n,f,u);total++;}}
 printf("PASS owned byte arrays: %d C/JIT cases, zero initialization, NUL/slices, allocation failures and complete descriptor/root retirement\n",total);return 0;}
