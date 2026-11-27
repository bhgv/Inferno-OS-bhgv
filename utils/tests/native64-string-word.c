#include "full.h"
static void wgcheck(int ok,const char*why){if(!ok){fprintf(stderr,"String Word predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);error(exModule);return H;}
static int parsefail,calls;static void*lastdest;static uchar olddest[4];
static void parsehandler(void){calls++;lastdest=R.d;memmove(olddest,R.d,4);if(parsefail){parsefail=0;failat=allocations+1;}actual_cvtcw();failat=0;}
static Module*wgroot(int grow){Module*m;Inst*p;uchar map[]={0,0xc0};m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("window-caller");m->path=strdup("window-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);require(m->type[0]&&m->type[1]);p=m->prog=mallocz(4*sizeof(Inst),1);require(p!=nil);m->entry=p;m->nprog=4;m->cstep=native64typedlea8step;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=72;p[0].d.ind=40;p[1].op=IMOVP;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;p[2].op=IMCALL;p[2].add=SRC(AFP)|AXIMM|DST(AFP);p[2].s.ind=40;p[2].d.ind=72;p[3].op=IEXIT;p[3].add=SRC(AXXX)|DST(AXXX);return m;
}
static String*seedstring(uint row,int*needs,int*length){String*s;const char*texts[]={"","","123"," -123tail","2147483647","-2147483648","+17","010","42\xc3\xa9","42\0junk"};uint i;*needs=0;*length=0;if(!row)return H;
 if(row<10){if(row==8)s=c2string((char*)texts[row],strlen(texts[row]));else{uint len=row==9?7:strlen(texts[row]);s=newstring(len+1);s->len=len;memmove(s->Sascii,texts[row],len);s->Sascii[len]=0;}*length=s->len<0?-s->len:s->len;*needs=s->len<0;return s;}
 if(row==15){s=newstring(3);s->len=2;memmove(s->Sascii,"77X",3);*length=2;return s;}
 if(row==14){const char*text="-2147483648";s=newrunes(11);for(i=0;i<11;i++)s->Srune[i]=text[i];}
 else{s=newrunes(row==13?2:3);if(row==13){s->Srune[0]=0x3bb;s->Srune[1]='7';}else for(i=0;i<3;i++)s->Srune[i]='1'+i;}
 if(row==11||row==12){s->tmp=mallocz(row==11?64:1,0);require(s->tmp!=nil);}*needs=row!=11;*length= -s->len;return s;
}
static void wgcase(uint fn,int jit,int quantum,uint row,int fault){Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};String*s;Prog*p;uchar*root,payload[128];void*cache;WORD values[]={0,0,123,-123,2147483647,(-2147483647-1),17,10,42,42,123,123,123,0,(-2147483647-1),77},actual,marker=0x31592653,after=0x10203040;UWORD expected;uint i,steps,bytes;int done,needs,length,failed,expectedcalls,nativefast;
 calls=0;parsefail=fault;lastdest=nil;wgcheck(native64typedmodule(&full_spec,jit,&owner),"source module");s=seedstring(row,&needs,&length);failed=fault&&needs;if(s!=H){ptradd(D2H(s));bytes=s->len<0?-s->len*sizeof(Rune):s->len;wgcheck(bytes<=sizeof(payload),"bounded payload");memmove(payload,s->Sascii,bytes);cache=s->tmp;}else{bytes=0;cache=nil;}
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);memmove(ml->MP+parse_globals[1],&marker,4);memmove(ml->MP+parse_globals[2],&after,4);
 m=wgroot(row&1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+72,&ml,8);if(s!=H)D2H(s)->ref++;memmove(root+64,&s,8);memset(root+80,0x5a,48);p->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}failat=0;
 if(steps>=10000||strcmp(worker.env->errstr,failed?exNomem:""))fprintf(stderr,"String Word fn=%u jit=%d q=%d row=%u fault=%d error=%s\n",fn,jit,quantum,row,fault,worker.env->errstr);wgcheck(steps<10000&&!strcmp(worker.env->errstr,failed?exNomem:""),"exact parsing completion/error");nativefast=jit&&(row<=2 || (row>=4&&row<=7));
#ifdef DIS64_TEST_RISCV64
 nativefast=0; /* RV64 currently uses the existing C tail for decimal conversion. */
#endif
 expectedcalls=nativefast?0:failed?1:fn==3?2:1;
#ifndef DIS64_TEST_RISCV64
 if(jit&&row==15)expectedcalls=1; /* First C conversion terminates; repeat can then use the leaf. */
#endif
 if(calls!=expectedcalls)fprintf(stderr,"parse count fn=%u jit=%d row=%u fault=%d needs=%d calls=%d\n",fn,jit,row,fault,needs,calls);wgcheck(calls==expectedcalls,"native decimal leaf or exact C fallback count");expected=failed?(UWORD)marker:fn==3?(UWORD)values[row]*2:fn==4?(values[row]>0?1:values[row]<0?(UWORD)-1:0):(UWORD)values[row];memmove(&actual,ml->MP+parse_globals[1],4);wgcheck((UWORD)actual==expected,"independent decimal Word32 result");memmove(&actual,ml->MP+parse_globals[2],4);wgcheck(actual==(!failed&&fn==0?length:after),"four-byte conversion preserves adjacent local");if(failed)wgcheck(lastdest!=nil&&!memcmp(lastdest,olddest,4),"cache failure preserves conversion destination");if(row==15)wgcheck(s->Sascii[2]==0,"unterminated ASCII falls back before native publication");if(s!=H){wgcheck(!memcmp(s->Sascii,payload,bytes),"String payload preserved");if(failed||row==11)wgcheck(s->tmp==cache,"previous UTF cache preserved or reused");if(!failed&&needs)wgcheck(s->tmp!=nil,"owned UTF cache published");}
 for(i=80;i<128;i++)wgcheck(root[i]==0x5a,"caller guard");observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);wgcheck(native64typedmoduleclear(&owner),"module retired");if(s!=H){wgcheck(D2H(s)->ref==1,"String fixture owner");ptrdel(D2H(s));destroy(s);}wgcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1&&Tstring.ref==1,"complete String/cache/frame retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void wgrefusals(void){Native64ModuleSpec spec;Native64ModuleFunction f[7];Native64TypedStep code[64];Native64ModuleOwner owner={0};uchar fmap[16],dmap[16];REG saved=R;uint fn,k,pc,bit,mode,bytes;int before,total=0;
 for(fn=0;fn<5;fn++)for(k=0;k<16;k++){spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;for(pc=0;pc<f[fn].count;pc++)if(code[pc].op==ICVTCW)break;require(pc<f[fn].count);memset(fmap,0,sizeof(fmap));memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;
 switch(k){case 0:code[pc].s++;break;case 1:code[pc].d++;break;case 2:code[pc].m=1;break;case 3:code[pc].mode|=AXIMM;break;case 4:code[pc].returned=4;break;case 5:code[pc].mode=SRC(AIMM)|DST(AFP);break;case 6:code[pc].mode=SRC(AFP)|DST(AMP);break;case 7:code[pc].s=-8;break;case 8:code[pc].d=32;break;case 9:code[pc].d=f[fn].bytes;break;
 case 10:bit=code[pc].s/8;mode=USRC(code[pc].mode);if(mode==AFP)fmap[bit/8]&=~(128>>(bit%8));else dmap[bit/8]&=~(128>>(bit%8));break;
 case 11:bit=code[pc].d/8;fmap[bit/8]|=128>>(bit%8);break;case 12:code[pc].s=USRC(code[pc].mode)==AFP?f[fn].bytes:spec.databytes;break;case 13:code[pc].op=ICVTWC;break;
 case 14:bytes=f[fn].bytes;f[fn].bytes+=64;f[fn].mapbytes=(f[fn].bytes+63)/64;code[pc].mode=SRC(AFP)|DST(AFP);code[pc].s=bytes+8;break;
 case 15:bytes=f[fn].bytes;f[fn].bytes+=64;f[fn].mapbytes=(f[fn].bytes+63)/64;code[pc].d=bytes+4;bit=code[pc].d/8;fmap[bit/8]|=128>>(bit%8);break;}
 before=allocations;wgcheck(!native64typedmodule(&spec,0,&owner),"malformed String Word metadata refused");wgcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"refusal before allocation");total++;}
 printf("PASS String Word: %u metadata refusals before allocation\n",total);
}
static void wgconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,n,i,base;
 for(jit=0;jit<2;jit++){base=allocations;wgcheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;wgcheck(native64typedmoduleclear(&owner),"constructor clear");for(i=1;i<=n;i++){failat=allocations+i;wgcheck(!native64typedmodule(&full_spec,jit,&owner),"constructor failure");failat=0;wgcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}printf("PASS String Word constructor jit=%d: %d failure points\n",jit,n);}
}
int main(void){uint fn,row,total=0;int jit,q,fault,quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freestring;require(dis64selectlaunch("dis64-calls"));
 PARSE_HANDLERS
 optab[IMOVP]=actual_movp;optab[ILENC]=actual_lenc;optab[ICVTCW]=parsehandler;wgrefusals();wgconstruction();for(fn=0;fn<6;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(row=0;row<16;row++)for(fault=0;fault<2;fault++){wgcase(fn,jit,quanta[q],row,fault);total++;}printf("PASS String Word: %u C/JIT runs; real cvtcw/string2c, nil/decimal/int32/NUL/Rune/cache, FP/MP/alias/repeat/branch/local CALL, cache failures and complete ownership retirement\n",total);return 0;}
