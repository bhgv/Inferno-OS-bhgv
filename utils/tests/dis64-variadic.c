#include <stdio.h>
static void varircheck(int ok,char*why){if(!ok){fprintf(stderr,"variadic IR: %s\n",why);exit(10);}}
/* These raw scalar/String IR cases have no implementation module or nominal
 * ADTs. Real source gates link the full compiler's impdecl and tequal instead. */
Decl *impdecl;
int tequal(Type*a,Type*b){USED(a);USED(b);varircheck(0,"unexpected nominal type comparison");return 0;}
static void stringflowcheck(void){
 Type text={0},word={0},fn={0};Decl arg={0},global={0},caller={0},*functions[]={&caller};Desc frame={0},data={0};uchar fm[]={0,128},dm[]={128};Inst code[6]={{0}};Dis64TypedBody p={0};int i,j,branches[]={IBEQC,IBNEC,IBLTC,IBLEC,IBGTC,IBGEC};
 text.kind=Tstring;text.size=text.align=4;word.kind=Tint;word.size=word.align=4;fn.kind=Tfn;fn.tof=&word;fn.ids=&arg;arg.ty=&text;arg.store=Darg;arg.offset=32;global.ty=&text;global.store=Dglobal;
 frame.size=40;frame.map=fm;frame.nmap=2;data.size=4;data.map=dm;data.nmap=1;descriptors=&data;caller.ty=&fn;caller.desc=&frame;caller.pc=code;
 for(i=0;i<6;i++){code[i].pc=i;code[i].op=INOP;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<5?code+i+1:nil;}
 code[0].op=IBEQC;code[0].sm=Afp;code[0].s.reg=32;code[0].s.decl=&arg;code[0].mm=Amp;code[0].m.decl=&global;code[0].dm=Apc;code[0].d.offset=4;code[0].branch=code+4;
 code[1].op=IMOVW;code[1].sm=Aimm;code[1].s.offset=1;code[1].dm=Afpind;code[1].d.reg=16;
 code[2].op=IJMP;code[2].dm=Apc;code[2].d.offset=5;code[2].branch=code+5;code[5].op=IRET;
 for(j=0;j<6;j++){code[0].op=branches[j];code[4].op=INOP;code[4].sm=code[4].mm=code[4].dm=Anone;code[4].d.reg=code[4].s.offset=0;
 varircheck(!dis64typedloads(&caller,&global,functions,1,code,6,6,&p)&&p.reason&&!strcmp(p.reason,"undefined-result"),"String branch cannot skip result definition");dis64typedfree(&p);
 code[4]=code[1];code[4].pc=4;code[4].next=code+5;code[4].s.offset=2;
 varircheck(dis64typedloads(&caller,&global,functions,1,code,6,6,&p),"String branch joins two defined results");dis64typedfree(&p);}
 global.ty=&word;dm[0]=0;varircheck(!dis64typedloads(&caller,&global,functions,1,code,6,6,&p),"String comparison rejects scalar middle");dis64typedfree(&p);
}
int main(void){
 Type text={0},module={0},word={0},fn={0},actual={0},callerfn={0},ref={0};Decl formal={0},arg={0},temp={0},scratch={0},global={0},caller={0},callee={0},parent={0},iface={0},frameDecl={0},*fns[]={&caller};
 Sym sys={0},name={0},tmpname={0};Desc cd={0},fd={0},data={0};uchar cm[]={0,0xc0},fm[]={0,0x80},dm[]={0x80};Inst code[6]={{0}},saved;Dis64TypedBody p={0};int i;
 text.kind=Tstring;text.size=text.align=4;module.kind=Tmodule;module.size=module.align=4;word.kind=Tint;word.size=word.align=4;ref.kind=Tref;ref.size=ref.align=4;
 formal.ty=&text;formal.store=Dfield;formal.offset=32;actual.kind=fn.kind=callerfn.kind=Tfn;actual.varargs=fn.varargs=1;actual.ids=fn.ids=&formal;actual.tof=fn.tof=callerfn.tof=&text;fn.sig=0x4c0624b6U;
 sys.name="Sys";sys.len=3;name.name="sprint";name.len=6;tmpname.name=".t0";tmpname.len=3;parent.sym=&sys;callee.sym=&name;callee.dot=&parent;callee.ty=&fn;callee.iface=&iface;
 arg.ty=&module;arg.offset=32;arg.store=Darg;temp.ty=&text;temp.offset=36;temp.store=Dlocal;temp.sym=&tmpname;temp.next=&scratch;scratch.ty=&word;scratch.offset=40;scratch.store=Dlocal;global.ty=&text;global.store=Dglobal;
 cd.id=1;cd.size=48;cd.map=cm;cd.nmap=2;fd.id=2;fd.size=36;fd.map=fm;fd.nmap=2;data.size=4;data.map=dm;data.nmap=1;descriptors=&data;frameDecl.desc=&fd;
 callerfn.ids=&arg;caller.ty=&callerfn;caller.locals=&temp;caller.desc=&cd;caller.pc=code;
 for(i=0;i<6;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<5?code+i+1:nil;}
 code[0].op=IFRAME;code[0].sm=Adesc;code[0].s.decl=&frameDecl;code[0].s.offset=2;code[0].dm=Afp;code[0].d.reg=40;code[0].native64args=&actual;
 code[1].op=IMOVP;code[1].sm=Amp;code[1].s.decl=&global;code[1].dm=Afpind;code[1].d.reg=40;code[1].d.offset=32;
 code[2].op=ILEA;code[2].sm=Afp;code[2].s.reg=36;code[2].s.decl=&temp;code[2].dm=Afpind;code[2].d.reg=40;code[2].d.offset=16;
 code[3].op=IMCALL;code[3].sm=Afp;code[3].s.reg=40;code[3].mm=Aoff;code[3].m.decl=&callee;code[3].dm=Afp;code[3].d.reg=32;code[3].d.decl=&arg;
 code[4].op=IMOVP;code[4].sm=Afp;code[4].s.reg=36;code[4].s.decl=&temp;code[4].dm=Afpind;code[4].d.reg=16;code[5].op=IRET;
 varircheck(dis64typedloads(&caller,&global,fns,1,code,6,6,&p)&&p.nvariadic==1&&p.variadic[0].frame.bytes==72&&p.variadic[0].count==0,"complete empty-tail sprint");dis64typedfree(&p);
 varircheck(!dis64typedexternal(&caller,&global,fns,1,code,6,6,&p),"ordinary profile closed");dis64typedfree(&p);
 for(i=0;i<10;i++){
  saved=code[i==0?0:i<4?1:2];
  if(i==0)code[0].native64args=nil;
  if(i==1)code[1].sm=Aimm;
  if(i==2)code[1].d.offset=36;
  if(i==3)code[1].branch=code+4;
  if(i==4)code[2].s.reg=40;
  if(i==5)code[2].d.offset=20;
  if(i==6)fn.sig^=1;
  if(i==7)fm[1]=0;
  if(i==8)formal.ty=&word;
  if(i==9)code[2].mm=Afp;
  varircheck(!dis64typedloads(&caller,&global,fns,1,code,6,6,&p),i==6?"signature refused":"malformed annotation/map/argument/result refused");dis64typedfree(&p);
  code[i==0?0:i<4?1:2]=saved;fn.sig=0x4c0624b6U;fm[1]=0x80;formal.ty=&text;
 }
 /* The actual per-use type refines only compiler temporary roots in LOAD. */
 code[0].op=IMFRAME;code[0].native64args=nil;code[0].sm=Afp;code[0].s.reg=32;code[0].s.decl=&arg;code[0].s.offset=0;code[0].mm=Aoff;code[0].m.decl=&callee;
 fn.varargs=0;fn.ids=nil;code[1]=code[2];code[1].pc=1;code[1].next=code+2;code[2]=code[3];code[2].pc=2;code[2].next=code+3;code[3]=code[4];code[3].pc=3;code[3].next=code+4;code[4]=code[5];code[4].pc=4;code[4].next=nil;
 temp.ty=&ref;code[1].s.native64type=code[3].s.native64type=&text;
 {int accepted=dis64typedloads(&caller,&global,fns,1,code,5,5,&p);if(!accepted)fprintf(stderr,"temporary refinement pc=%u reason=%s\n",p.badpc,p.reason);varircheck(accepted,"reused managed temporary per-use String type");}dis64typedfree(&p);
 varircheck(!dis64typedexternal(&caller,&global,fns,1,code,5,5,&p),"temporary refinement LOAD-only");dis64typedfree(&p);
 tmpname.name="named";varircheck(!dis64typedloads(&caller,&global,fns,1,code,5,5,&p),"ordinary local cannot refine nominal root type");dis64typedfree(&p);tmpname.name=".t0";
 code[1].s.native64type=&word;varircheck(!dis64typedloads(&caller,&global,fns,1,code,5,5,&p),"scalar annotation cannot refine a root");dis64typedfree(&p);
 stringflowcheck();varircheck(!owners,"all compiler owners retired");puts("PASS variadic IR: exact instantiated layout, ten annotation/hash/map/operand refusals, temporary per-use types and old-profile closure");return 0;
}
